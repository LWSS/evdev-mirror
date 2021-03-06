#include <linux/ftrace.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/input.h>
#include <asm/uaccess.h> //copy_from_user
#include <linux/major.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <asm/spinlock.h>
#include <linux/kprobes.h>

#include "kallsyms.h"

MODULE_DESCRIPTION("Mirror evdev events to a file.");
MODULE_AUTHOR("LWSS/Heep");
MODULE_LICENSE("GPL");
MODULE_INFO(livepatch, "Y");

//#define OUTPUT_MOUSE_EVENTS 1

#define kprint(fmt, ...) printk( (KBUILD_MODNAME ": "fmt), ##__VA_ARGS__ );

#ifndef CONFIG_X86_64
    #error Only x86_64 architecture is supported!
#endif

/*
 * There are two ways of preventing vicious recursive loops when hooking:
 * - detect recusion using function return address (USE_FENTRY_OFFSET = 0)
 * - avoid recusion by jumping over the ftrace call (USE_FENTRY_OFFSET = 1)
 */
#define USE_FENTRY_OFFSET 0


/*
 * Tail call optimization can interfere with recursion detection based on
 * return address on the stack. Disable it to avoid machine hangups.
 */
#if !USE_FENTRY_OFFSET
    #pragma GCC optimize("-fno-optimize-sibling-calls")
#endif

static DEFINE_MUTEX(data_mutex);

struct ftrace_hook {
    const char *name;
    void *function;
    void *original;

    unsigned long address;
    struct ftrace_ops ops;
};

struct device dev;
struct cdev cdev;

spinlock_t input_lock;
static struct input_value last_event;
bool fresh = false;

static struct ftrace_hook evdev_events_hook;
static struct input_handle *last_handle = 0;



static int init_hook( struct ftrace_hook *hook )
{
#if USE_FENTRY_OFFSET
    *((unsigned long*) hook->original) = hook->address + MCOUNT_INSN_SIZE;
#else
    *((unsigned long*) hook->original) = hook->address;
#endif

    return 0;
}

static void notrace ftrace_thunk(unsigned long ip,
                                 unsigned long parent_ip,
                                 struct ftrace_ops *ops,
                                 struct pt_regs *regs)
{
    struct ftrace_hook *hook = container_of(ops, struct ftrace_hook, ops);

#if USE_FENTRY_OFFSET
    regs->ip = (unsigned long) hook->function;
#else
    if (!within_module(parent_ip, THIS_MODULE))
        regs->ip = (unsigned long) hook->function;
#endif
}

static asmlinkage void (*orig_evdev_events)(struct input_handle *handle,
                                            const struct input_value *vals,
                                            unsigned int count);

static asmlinkage void hooked_evdev_events(struct input_handle *handle,
                                           const struct input_value *vals,
                                           unsigned int count)
{
    int i;
    for( i = 0; i < count; i++ ){
#ifdef OUTPUT_MOUSE_EVENTS
        /* We don't care about anything except for keypresses/mouse/touchpad */
        if( vals[i].type != EV_KEY && vals[i].type != EV_REL && vals[i].type != EV_ABS ){
            continue;
        }
        if( vals[i].type == EV_REL ){
            last_handle = handle; // save mouse
        }
#else
        if( vals[i].type != EV_KEY ){
            continue;
        }
#endif
        spin_lock(&input_lock); // This could cause small input lag? Maybe add a buffer
        last_event = vals[i];
        fresh = true;
        spin_unlock(&input_lock);
        //kprint("Event #(%d): type: %d - code: %d - value: %d\n", i, vals[i].type, vals[i].code, vals[i].value);
    }
    orig_evdev_events( handle, vals, count );
}

static ssize_t mirror_read(struct file *file,
                           char *user_buffer,
                           size_t count,
                           loff_t *ppos)
{
    if( !fresh )
        return 0;

    if( count < sizeof(struct input_value) ){
        kprint("ERROR: Userspace buffer smaller than input event!\n");
        return -EFAULT;
    }

    spin_lock(&input_lock);
    //kprint("mirror_read: input code: (%d/%ld)\n", last_event.code, sizeof(struct input_value));
    if( copy_to_user(user_buffer, &last_event, sizeof(struct input_value)) ){
        kprint("ERROR: Copying to User failed\n");
        return -EFAULT;
    }

    fresh = false;
    spin_unlock(&input_lock);

    return sizeof(struct input_value);
}

static ssize_t mirror_write(struct file *file,
                            const char *user_buffer,
                            size_t count,
                            loff_t *ppos){
    if( count != sizeof(struct input_value) || *ppos > 0 ){
        kprint("ERROR: Malformed input_value\n");
        return -EFAULT;
    }
    if( !last_handle ){
        kprint("Move your mouse first\n");
        return -EFAULT;
    }
    struct input_value input;
    copy_from_user(&input, user_buffer, sizeof(struct input_value));
    //kprint("injecting event type:(%d) - code(%d) - value(%d)\n", input.type, input.code, input.value);

    input_inject_event(last_handle, input.type, input.code, input.value);
    input_sync(last_handle->dev);

    return sizeof(struct input_value);
}

static const struct file_operations mirrordev_fops = {
        .owner		= THIS_MODULE,
        .read		= mirror_read,
        .write      = mirror_write
};

static void mirrordev_release(struct device *dev)
{

}

static int startup(void)
{
    if( init_kallsyms() ){
        kprint( "Error initing kallsyms hack.\n" );
        return -EAGAIN;
    }

    evdev_events_hook.name = "evdev_events";
    evdev_events_hook.address = kallsyms_lookup_name( "evdev_events" );
    if( !evdev_events_hook.address ){
        kprint( "Error getting evdev_events addr! (%p)\n", evdev_events_hook.address );
        return -EAGAIN;
    }

    if( !evdev_events_hook.address ){
        kprint( "Error resolving the Address\n" );
        return -ENXIO;
    }

    evdev_events_hook.function = hooked_evdev_events;
    evdev_events_hook.original = &orig_evdev_events;
    int ret = init_hook( &evdev_events_hook );

    if( ret )
        return ret;

    evdev_events_hook.ops.func = ftrace_thunk;
    evdev_events_hook.ops.flags = FTRACE_OPS_FL_SAVE_REGS
                                  | FTRACE_OPS_FL_RECURSION_SAFE
                                  | FTRACE_OPS_FL_IPMODIFY;

    ret = ftrace_set_filter_ip(&evdev_events_hook.ops, evdev_events_hook.address, 0, 0);
    if( ret ){
        kprint("ftrace_set_filter_ip() failed: %d\n", ret);
        return ret;
    }

    ret = register_ftrace_function(&evdev_events_hook.ops);
    if (ret) {
        kprint("register_ftrace_function() failed: %d\n", ret);
        ftrace_set_filter_ip(&evdev_events_hook.ops, evdev_events_hook.address, 1, 0);
        return ret;
    }

    spin_lock_init(&input_lock);

    dev_set_name(&dev, "evdev-mirror");
    dev.class = &input_class;
    dev.devt = MKDEV(INPUT_MAJOR, input_get_new_minor(63, 1, false));
    dev.release = mirrordev_release;
    ret = device_register(&dev);
    if (ret) {
        kprint("device_register failed: %d\n", ret);
        return ret;
    }

    cdev_init(&cdev, &mirrordev_fops);
    cdev.kobj.parent = &dev.kobj;
    ret = cdev_add(&cdev, dev.devt, 1);
    if (ret) {
        kprint("cdev_add failed: %d\n", ret);
        return ret;
    }

    kprint("Loading complete.\n");
    return 0;
}

static void shutdown(void)
{
    int ret;
    ret = unregister_ftrace_function(&evdev_events_hook.ops);
    if( ret )
        kprint("unregister_ftrace_function() failed: %d\n", ret);

    ret = ftrace_set_filter_ip(&evdev_events_hook.ops, evdev_events_hook.address, 1, 0);
    if( ret )
        kprint("ftrace_set_filter_ip() failed: %d\n", ret);

    cdev_del(&cdev);
    device_del(&dev);
    //input_free_minor(MINOR(dev.devt));
    //put_device(&dev);

    kprint("UnLoaded.\n");
}

module_init(startup);
module_exit(shutdown);