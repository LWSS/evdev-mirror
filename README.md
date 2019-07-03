# _Experimental_ evdev input mirror
When qemu is setup to use exclusive evdev input, it's actually a real pain to read the inputs to your vm from your host machine.

If you don't want to rebuild qemu or linux from source, you're kinda stuck with hooking(maybe if ur a big brain you can do something like register another input listener in the kernel but I tried that and it no work). I think hooking the kernel is better because it seems to almost never change and the symbols are exported with kallsyms.

This is setup just for keypresses/mousebuttons at the moment.

## System Requirements ( approx )
* 64bit Linux system
* Kernel Version >= 3.19( for FTRACE_OPS_FL_IPMODIFY )
* The following Kernel Build configurations 
	* CONFIG_FTRACE
	* CONFIG_KALLSYMS
	* CONFIG_DYNAMIC_FTRACE_WITH_REGS
	* CONFIG_HAVE_FENTRY
	
Your distro provider probably put a list of your config options in `/boot/config*`, there's a good chance your kernel already has these options, but if it does not, you'll have to rebuild from source.
* Kernel headers for your current kernel.
* elfutils development package ( "elfutils-libelf-devel" for redhat, "libelf-dev" for ubuntu )
* Development Essentials ( make, gcc, etc. )
## Build Instructions
*  After installing kernel headers, you should just be able to use the makefile.
* `make` in the cartographer directory.

## Why did you take out the timestamp that the other evdev events have
Because that recently got changed in the linux kernel. My VM was on 4.19 and i'm on 4.20 and I was wondering why it wasn't working. ( it was mismatched )

I figured that I didn't really need the timestamp anyway.

The struct is still a standard linux struct, which is also in <linux/input.h>, but it's just an `input_value` instead of an `input_event` and the only difference is the missing time.

## Overview
* When inserted into the kernel, it will hook the evdev events function and create `/dev/input/evdev-mirror` character device
* You can now read keyboard events from that device, (see the example)

## Credits

-Alexey Lozovsky - For his series of articles [part1](https://www.apriorit.com/dev-blog/544-hooking-linux-functions-1) about ftrace and hooking with ftrace along with code snippets that I used in this project.
