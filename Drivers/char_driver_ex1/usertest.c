#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

static const char dev_file_name[] = "/dev/sp6";

int save_bitstream_version(char * bsv);
int print_bitstream_version();
int print_file_stats();

int main(int argc, char **argv) {
	(void)argc; (void)argv;
	print_file_stats(dev_file_name);
	save_bitstream_version("mais coucou mon petit");
	print_file_stats(dev_file_name);
	print_bitstream_version();
}

int save_bitstream_version(char * bsv) {
	int fd, ret;
	char rbsv[80];
	size_t sl = strlen(bsv);
	if (sl > 80) return -1;
	else if (sl < 80) { /* add padding to version string */
		memcpy(rbsv, bsv, sl);
		memset(&rbsv[sl], ' ', 79 - sl);
		rbsv[79] = 0;
	}
	printf("Setting bitstream version to : %s\n", rbsv);
	fd = open(dev_file_name, O_WRONLY);
	if(fd == -1) {
		printf("ERROR : cannot open file to write btstr version\n");
		return -1;
	}
	ret = write(fd, rbsv, strlen(rbsv));
	if(ret <= 0) {
		printf("ERROR : something went wrong while writing btstr version.\n"
			   "Return code is : %d\n", ret);
	}
	while(close(fd) != 0);
	return 0;
}

int print_bitstream_version() {
	int fd, ret;
	char rbsv[80];
	fd = open(dev_file_name, O_RDONLY);
	if(fd == -1) {
		printf("ERROR : cannot open file to read btstr version\n");
		return -1;
	}
	ret = read(fd, rbsv, 80);
	if(ret != 80) {
		printf("ERROR : problem while reading btstr version : %d\n", ret);
		while(close(fd) != 0);
		return -1;
	}
	printf("Bitstream version : %s\n", rbsv);
	while(close(fd) != 0);
	return 0;
}

int print_file_stats() {
	int ret;
	struct stat st;
	int fd = open(dev_file_name, O_RDONLY);
	if(fd == -1) {
		printf("%s\n", "ERROR : cannot open " 
					   "specified file to get stats. "
					   "Maybe does not exist");	
		return -1;
	}
	ret = fstat(fd, &st); 
	if(ret != 0) {
		printf("%s\n", "ERROR : cannot get file stats");
		return -1;
	}
	printf("Device ID : %ld\n"
		   "Inode number : %ld\n"
		   "Protection mode : %d\n"
		   "Num of hard links : %ld\n"
		   "User ID of owner : %d\n"
		   "Group ID of owner : %d\n"
		   "Device ID (spec files only): %ld\n"
		   "Total size [bytes] : %ld\n", 
		st.st_dev, 
		st.st_ino,
		st.st_mode,
		st.st_nlink,
		st.st_uid,
		st.st_gid,
		st.st_rdev,
		st.st_size);
		while(close(fd) != 0);
		return 0;
}