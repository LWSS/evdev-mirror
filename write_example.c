// An example to inject input events by using evdev-mirror kernel module

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
#define EV_ABS	        0x03

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
    fd = open("/dev/input/evdev-mirror", O_WRONLY);
    if( fd < 0 ){
        printf("Error opening evdev-mirror!(%d)\n", fd);
        return -1;
    }

    input.type = EV_REL;
    input.value = 10;
    input.code = AXIS_X;

    printf("setting mouse right 10units.\n");

    write( fd, &input, sizeof(struct input_value) );

    return 0;
}
