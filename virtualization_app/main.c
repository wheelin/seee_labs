#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

const char input_file_name[] = "/dev/input/event0";
const char * led_file_names[3] = {
	"/sys/class/leds/sp6_led0/brightness",
	"/sys/class/leds/sp6_led1/brightness",
	"/sys/class/leds/sp6_led2/brightness"
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
	int fd, rd;
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
		if(ev[0].value != ' ' && ev[1].value == 1 && ev[1].type == 1)
		{
			printf("\t := %d\n", ev[1].code);
		}
	}
}