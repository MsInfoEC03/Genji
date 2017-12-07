#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define LENGTH 100
#define MEM_CLEAR 0x01

int main()
{
	int fd,n,pos;
	char buf[LENGTH] = "hello world\n";
	fd = open("/dev/globalfifo",O_RDWR);
	if(fd < 0)
	{
		printf("globalmem open error.\n");
		exit(0);
	}
	else
	{
		printf("globalmem is opened,fs is %d\n",fd);
		write(fd,buf,strlen(buf));
	}
	close(fd);
	return 0;
}