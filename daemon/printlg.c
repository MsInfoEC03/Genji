#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <string.h> 
#include <syslog.h>

void daemon_init(void)
{
    //1.重新设置umask
    umask(0);
    //2.脱离父进程
    pid_t pid = fork();

    if(pid < 0)
        exit(1);    //进程创建失败
    else if(pid > 0)
        exit(0);    //退出父进程
    //3.重启session会话
    setsid();
    //4.改变工作目录
    chdir("/");
    //5.得到并关闭文件描述符
    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE,&rl);
    if(rl.rlim_max == RLIM_INFINITY)
        rl.rlim_max == 1024;
    for(int i = 0;i < rl.rlim_max;i++)
        close(i);   
    //6.固定文件描述符0，1，2到/dev/null(不接受标准输入，输入，错误)
    int fd0 = open("/dev/null",O_RDWR);
    int fd1 = dup(0);
    int fd2 = dup(0);
}

int main(void)
{
    //初始化守护进程
    daemon_init();
    
    //守护进程逻辑
    char *msg = "I'm printlg process...\n";
    int msg_len = strlen(msg);

    int fd = open("/tmp/test_printlg.log",O_RDWR|O_CREAT|O_APPEND,0666);
    if(fd < 0)
    {
        //printf("open/tmp/test_printlg.log fail.\n");
        syslog(LOG_ERR|LOG_USER,"open/tmp/test_printlg.log fail.\n");
        exit(1);
    }

    while(1)
    {
        //每隔三秒输出msg到test_printlg.log文件中
        syslog(LOG_ERR|LOG_USER,"syslog come from printlg process.\n");        
        write(fd,msg,msg_len);
        sleep(3);
    }
    
    close(fd);

    return 0;
}