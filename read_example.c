// An example to read from the evdev-mirror kernel module
//
// Note: The module will only output EV_KEY events (keypresses)

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

// This struct is the same at <linux/input.h>
struct input_value {
    uint16_t type;
    uint16_t code;
    uint32_t value;
};


int main( int argc, char **argv )
{
    int fd;
    struct input_value input;

    if( geteuid() != 0 ){
        printf("Run as root!\n");
        return -2;
    }
    fd = open("/dev/input/evdev-mirror", O_RDONLY);
    if( fd < 0 ){
        printf("Error opening evdev-mirror!(%d)\n", fd);
        return -1;
    }

    while( 1 ){
        // if zero bytes, keep goin...
        if( !read( fd, &input, sizeof(struct input_value) ) )
            continue;

        printf("Key: %d - state: %d\n", input.code, input.value);
    }
}