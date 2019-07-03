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
    int32_t value;
};

#define EV_KEY			0x01
#define EV_REL	        0x02

// Made up
#define AXIS_X          0x00 // left negative - right positive
#define AXIS_Y          0x01 // up negative - down positive
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
        if( input.type == EV_KEY ){
            printf("KeyPress: %d - state: %d\n", input.code, input.value);
        } else if( input.type == EV_REL ){
            printf("MouseMove: axis(%s) units(%d) ", input.code ? "Y" : "X", input.value);
            if( input.code ){
                printf( "%s\n", (input.value > 0) ? "DOWN" : "UP" );
            } else {
                printf( "%s\n", (input.value > 0) ? "RIGHT" : "LEFT" );
            }
        } else {
            printf("unknown input?\n");
        }
    }
}