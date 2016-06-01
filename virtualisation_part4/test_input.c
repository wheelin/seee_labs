#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

/*
	bottom : 108
	right : 105
	center : 28
	left : 106
*/
#define BOTTOM_BUT 108
#define RIGHT_BUT 106
#define CENTER_BUT 28
#define LEFT_BUT 105


const char input_file_name[] = "/dev/input/event0";

#define LED_NUM 3
const char * led_file_names[3] = {
	"/sys/class/leds/led0/brightness",
	"/sys/class/leds/led1/brightness",
	"/sys/class/leds/led2/brightness"
};

int set_led(int led, int state)
{
	int fd, wr;
	if(led < 0 || led > 2) return -1;
	fd = open(led_file_names[led], O_WRONLY);
	if(fd == -1) return -1;
	if(state) 
	{	
		wr = write(fd, "1", 1);
		if(wr < 1)
		{
			perror("Error writing led\n");
			return -1;
		}
	}
	else
	{
		wr = write(fd, "0", 1);
		if(wr < 1)
		{
			perror("Error writing led\n");
			return -1;
		}
	} 
	close(fd);
	return 0;
}

int main(int argc, char **argv)
{
	struct input_event ev[64];
	int fd, rd, i;
	int size = sizeof(struct input_event);
	fd = open(input_file_name, O_RDONLY);
	if(fd == -1)
	{
		perror("Problem opening input file\n");
		return -1;
	}

	while(1)
	{
		if((rd = read(fd, ev, size * 64)) < size)
		{
			perror("Problem reading input\n");
			close(fd);
			return -1;
		}
		if(ev[0].value == 1 && ev[0].type == EV_KEY)
		{
			printf("\t := %d\n", ev[0].code);
			switch (ev[0].code) 
			{
				case RIGHT_BUT:
					set_led(0, 1);
					break;
				case CENTER_BUT:
					set_led(1, 1);
					break;
				case LEFT_BUT:
					set_led(2, 1);
					break;
				case BOTTOM_BUT:
					for(i = 0; i < LED_NUM; i++)
						set_led(i, 0);
					break;
				//default:
					// nop
			}
		}
	}
}