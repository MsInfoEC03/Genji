 #include <unistd.h>  
 #include <stdio.h>  
 #include <stdlib.h>  
 #include <string.h>  
 #include <fcntl.h>  
 #include <linux/fb.h>  
 #include <sys/mman.h>  
 #include <sys/ioctl.h>   
   
 #define PAGE_SIZE 4096  
   
   
 int main(int argc , char *argv[])  
 {  
     int fd;  
     int i;  
     unsigned char *p_map;  
       
     //打开设备  
     fd = open("/dev/globalmem",O_RDWR);  
     if(fd < 0)  
     {  
         printf("open fail\n");  
         exit(1);  
     }  
   
     //内存映射  
     p_map = (unsigned char *)mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,fd, 0);  
     if(p_map == MAP_FAILED)  
     {  
         printf("mmap fail\n");  
         goto here;  
     }  
   
     printf("virt = %lx\n",(unsigned long)p_map);

     //打印映射后的内存中的前10个字节内容  
     for(i=0;i<PAGE_SIZE;i++)
     {
         if(p_map[i] < '0'||p_map[i] > '9')
            continue;
         if(i % 50 == 0)
            printf("\n");
         printf("p_map[%d]=%c ",i,p_map[i]);          
     }  
     printf("\n");
   
 here:  
     munmap(p_map, PAGE_SIZE);  
     return 0;  
 }  