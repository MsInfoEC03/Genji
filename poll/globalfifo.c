/*
*a simple char device driver: globalfifo mutex
*/
#include<linux/module.h>
#include<linux/fs.h>
#include<linux/init.h>
#include<linux/cdev.h>
#include<linux/slab.h>
#include<linux/uaccess.h>
#include<linux/mm.h>
#include<linux/mutex.h>
#include<asm/atomic.h>
#include<linux/wait.h>
#include<linux/sched.h>
#include<linux/poll.h>

#define GLOBALFIFO_SIZE 0x1000
#define MEM_CLEAR 0x1
#define GLOBALFIFO_MARJOR 230

static unsigned char array[10]={'0','1','2','3','4','5','6','7','8','9'};

static int globalfifo_major = GLOBALFIFO_MARJOR;
module_param(globalfifo_major,int,S_IRUGO);

struct globalfifo_dev{
	// struct cdev cdev;
	unsigned char mem[GLOBALFIFO_SIZE];
	unsigned int current_len;
	struct cdev cdev;
	struct mutex mutex;
	wait_queue_head_t r_wait;
	wait_queue_head_t w_wait;	
};

struct globalfifo_dev* globalfifo_devp;

static ssize_t globalfifo_read(struct file* filp,char __user * buf,size_t count,loff_t* ppos)
{	
	int ret = 0;
	struct globalfifo_dev* dev = filp->private_data;

	DECLARE_WAITQUEUE(wait,current);

	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->r_wait,&wait);

	while(dev->current_len == 0)
	{
		if(filp->f_flags & O_NONBLOCK)
		{
			ret = -EAGAIN;
			goto out;
		}
		__set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&dev->mutex);

		schedule();
		if(signal_pending(current))
		{
			ret = -ERESTARTSYS;
			goto out2;
		}

		mutex_lock(&dev->mutex);
	}

	if(count > dev->current_len)
		count = dev->current_len;
	if(copy_to_user(buf,dev->mem,count))
	{
		ret = -EFAULT;
		goto out;
	}
	else
	{
		memcpy(dev->mem,dev->mem + count,dev->current_len - count);
		dev->current_len -= count;
		printk(KERN_INFO "read %d bytes,current_len:%d\n",count,dev->current_len);

		wake_up_interruptible(&dev->w_wait);

		ret = count;
	}
	 out:
	mutex_unlock(&dev->mutex);
	 out2:
	remove_wait_queue(&dev->w_wait,&wait);
	set_current_state(TASK_RUNNING);

	return ret;
}

static ssize_t globalfifo_write(struct file* filp,const char __user * buf,size_t count,loff_t* ppos)
{
	int ret = 0;

	struct globalfifo_dev* dev = filp->private_data;

	DECLARE_WAITQUEUE(wait,current);

	mutex_lock(&dev->mutex);
	add_wait_queue(&dev->w_wait,&wait);
	
	while(dev->current_len == GLOBALFIFO_SIZE)
	{
		if(filp->f_flags & O_NONBLOCK)
		{
			ret = -EAGAIN;
			goto out;
		}
		__set_current_state(TASK_INTERRUPTIBLE);

		mutex_unlock(&dev->mutex);

		schedule();
		if(signal_pending(current))
		{
			ret = -ERESTARTSYS;
			goto out2;
		}

		mutex_lock(&dev->mutex);
	}

	if(count > GLOBALFIFO_SIZE - dev->current_len)
		count = GLOBALFIFO_SIZE - dev->current_len;

	if(copy_from_user(dev->mem + dev->current_len,buf,count))
	{
		ret = -EFAULT;
		goto out;
	}	
	else
	{
		dev->current_len += count;
		printk(KERN_INFO "written %d bytes,current_len:%d\n",count,dev->current_len);

		wake_up_interruptible(&dev->r_wait);

		ret = count;
	}

	 out:
	mutex_unlock(&dev->mutex);
	 out2:
	remove_wait_queue(&dev->w_wait,&wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

static loff_t globalfifo_llseek(struct file* filp,loff_t offset,int orig)
{
	loff_t ret = 0;
	switch(orig){
	case 0:/*从文件头位置seek*/
		if(offset < 0){
			ret = -EINVAL;
			break;
		}
		if((unsigned int)offset > GLOBALFIFO_SIZE){
			ret = -EINVAL;
			break;
		}
		filp->f_pos = (unsigned int)offset;
		ret = filp->f_pos;
		break;
	case 1:/*从文件当前位置开始seek*/
		if((filp->f_pos + offset) > GLOBALFIFO_SIZE){
			ret = -EINVAL;
			break;
		}
		filp->f_pos += offset;
		ret = filp->f_pos;
		break;
	default:
		ret = EINVAL;
		break;
	}
	return ret;
}

static long globalfifo_ioctl(struct file* filp,unsigned int cmd,unsigned long arg)
{
	struct globalfifo_dev* dev = filp->private_data;
	
	switch(cmd){
	case MEM_CLEAR:
		//mutex_lock(&dev->mutex);	
		memset(dev->mem,0,GLOBALFIFO_SIZE);
		//mutex_unlock(&dev->mutex);

		printk(KERN_INFO "globalfifo is set to zero\n");
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}

//static atomic_t scull_available = ATOMIC_INIT(1);      //init atomic

static int globalfifo_open(struct inode* inode,struct file* filp)
{
	filp->private_data = globalfifo_devp;
	//mutex_lock(&globalfifo_devp->mutex);

	// if(!atomic_dec_and_test(&scull_available)){
    //     atomic_inc(&scull_available);
    //     return -EBUSY;
    // }
	return 0;
}

static int globalfifo_release(struct inode* inode,struct file* filp)
{
	//mutex_unlock(&globalfifo_devp->mutex);

	// atomic_inc(&scull_available);

	return 0;
}

 static void my_vma_open(struct vm_area_struct *vma)
 {
    printk(KERN_NOTICE "my VMA open,virt %lx, size %lx, phys %lx\n",vma->vm_start,vma->vm_end - vma->vm_start,virt_to_phys(globalfifo_devp->mem));
 }

 static void my_vma_close(struct vm_area_struct *vma)
 {
     printk(KERN_NOTICE "my VMA close.\n");
 }

 static struct vm_operations_struct my_remap_vm_ops = {
     .open = my_vma_open,
     .close = my_vma_close,
 };

static int globalfifo_mmap(struct file *filp,struct vm_area_struct *vma)
{
     unsigned long page;  
     unsigned char i;  
     unsigned long start = (unsigned long)vma->vm_start;  
     //unsigned long end =  (unsigned long)vma->vm_end;  
     unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);  
   
     //得到物理地址  
     page = virt_to_phys(globalfifo_devp->mem);
     printk(KERN_NOTICE "phys = %lx\n",page);      
     //将用户空间的一个vma虚拟内存区映射到以page开始的一段连续物理页面上  
     if(remap_pfn_range(vma,start,page>>PAGE_SHIFT,size,PAGE_SHARED))//第三个参数是页帧号，由物理地址右移PAGE_SHIFT得到  
         return -1;  
      
     vma->vm_ops = &my_remap_vm_ops;
     my_vma_open(vma); 

     //往该内存写10字节数据  
     for(i=0;i<10;i++)  
         globalfifo_devp->mem[i] = array[i];  

    //  my_vma_close(vma);

     return 0;
}

static unsigned int globalfifo_poll(struct file *filp,poll_table *wait)
{
	unsigned int mask = 0;
	struct globalfifo_dev *dev = filp->private_data;

	mutex_lock(&dev->mutex);

	poll_wait(filp,&dev->r_wait,wait);
	poll_wait(filp,&dev->w_wait,wait);

	if(dev->current_len != 0)
	{
		mask |= POLLIN | POLLRDNORM;
	}

	if(dev->current_len != GLOBALFIFO_SIZE)
	{
		mask |= POLLOUT | POLLWRNORM;
	}

	mutex_unlock(&dev->mutex);
	return mask;
}

static const struct file_operations globalfifo_fops = {
	.owner = THIS_MODULE,
	.llseek = globalfifo_llseek,
	.read = globalfifo_read,
	.write = globalfifo_write,
	.unlocked_ioctl = globalfifo_ioctl,
	.open = globalfifo_open,
	.release = globalfifo_release,
	.mmap = globalfifo_mmap,
	.poll = globalfifo_poll,
};

static void __exit globalfifo_exit(void)
{
	cdev_del(&globalfifo_devp->cdev);
	kfree(globalfifo_devp);
	unregister_chrdev_region(MKDEV(globalfifo_major,0),1);
}
module_exit(globalfifo_exit);

static void globalfifo_setup_cdev(struct globalfifo_dev* dev,int index)
{
	int err,devno = MKDEV(globalfifo_major,index);

	cdev_init(&dev->cdev,&globalfifo_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev,devno,1);
	if(err)
		printk(KERN_NOTICE "Error %d adding globalfifo%d",err,index);
}

static int __init globalfifo_init(void)
{
	int ret;
	dev_t devno = MKDEV(globalfifo_major,0);
	
	if(globalfifo_major)
		ret = register_chrdev_region(devno,1,"globalfifo");
	else{
		ret = alloc_chrdev_region(&devno,0,1,"globalfifo");
	}
	if(ret < 0)
		return ret;
	
	globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev),GFP_KERNEL);
	if(!globalfifo_devp){
		ret = -ENOMEM;
		goto fail_malloc;
	}
	
	mutex_init(&globalfifo_devp->mutex);
	init_waitqueue_head(&globalfifo_devp->r_wait);
	init_waitqueue_head(&globalfifo_devp->w_wait);
	
	globalfifo_setup_cdev(globalfifo_devp,0);

	return 0;
	
	fail_malloc:
	unregister_chrdev_region(devno,1);
	return ret;
}
module_init(globalfifo_init);

MODULE_AUTHOR("bo wang");
MODULE_LICENSE("GPL v2");
