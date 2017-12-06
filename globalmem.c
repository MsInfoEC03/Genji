/*
*a simple char device driver: globalmem without mutex
*/
#include<linux/module.h>
#include<linux/fs.h>
#include<linux/init.h   >
#include<linux/cdev.h>
#include<linux/slab.h>
#include<linux/uaccess.h>
#include<linux/mm.h>
#include<linux/mutex.h>

#define GLOBALMEM_SIZE 0x1000
#define MEM_CLEAR 0x1
#define GLOBALMEM_MARJOR 230

static unsigned char array[10]={'0','1','2','3','4','5','6','7','8','9'};

static int globalmem_major = GLOBALMEM_MARJOR;
module_param(globalmem_major,int,S_IRUGO);

struct globalmem_dev{
	// struct cdev cdev;
	unsigned char mem[GLOBALMEM_SIZE];
	struct cdev cdev;
	struct mutex mutex;
};

struct globalmem_dev* globalmem_devp;

static ssize_t globalmem_read(struct file* filp,char __user * buf,size_t size,loff_t* ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct globalmem_dev* dev = filp->private_data;
	
	if(p >= GLOBALMEM_SIZE)
		return 0;
	if(count > GLOBALMEM_SIZE - p)
		count = GLOBALMEM_SIZE - p;
	
	mutex_lock(&dev->mutex);

	if(copy_to_user(buf,dev->mem + p,count)){
		ret = -EFAULT;
	}else{
		*ppos += count;
		ret = count;
		
		printk(KERN_INFO "read %u bytes(s) from %lu\n",count,p);
	}

	mutex_unlock(&dev->mutex);

	return ret;
}

static ssize_t globalmem_write(struct file* filp,const char __user * buf,size_t size,loff_t* ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct globalmem_dev* dev = filp->private_data;
	
	if(p >= GLOBALMEM_SIZE)
		return 0;
	if(count > GLOBALMEM_SIZE - p)
		count = GLOBALMEM_SIZE - p;
	
	mutex_lock(&dev->mutex);	

	if(copy_from_user(dev->mem + p,buf,count))
		ret = -EFAULT;
	else{
		*ppos += count;
		ret = count;
		
		printk(KERN_INFO "written %u bytes(s) from %lu\n",count,p);
	}
	
	mutex_unlock(&dev->mutex);	

	return ret;
}

static loff_t globalmem_llseek(struct file* filp,loff_t offset,int orig)
{
	loff_t ret = 0;
	switch(orig){
	case 0:/*从文件头位置seek*/
		if(offset < 0){
			ret = -EINVAL;
			break;
		}
		if((unsigned int)offset > GLOBALMEM_SIZE){
			ret = -EINVAL;
			break;
		}
		filp->f_pos = (unsigned int)offset;
		ret = filp->f_pos;
		break;
	case 1:/*从文件当前位置开始seek*/
		if((filp->f_pos + offset) > GLOBALMEM_SIZE){
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

static long globalmem_ioctl(struct file* filp,unsigned int cmd,unsigned long arg)
{
	struct globalmem_dev* dev = filp->private_data;
	
	switch(cmd){
	case MEM_CLEAR:
		mutex_lock(&dev->mutex);	
		memset(dev->mem,0,GLOBALMEM_SIZE);
		mutex_unlock(&dev->mutex);

		printk(KERN_INFO "globalmem is set to zero\n");
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}

static int globalmem_open(struct inode* inode,struct file* filp)
{
	filp->private_data = globalmem_devp;
	return 0;
}

static int globalmem_release(struct inode* inode,struct file* filp)
{
	return 0;
}

 static void my_vma_open(struct vm_area_struct *vma)
 {
    printk(KERN_NOTICE "my VMA open,virt %lx, size %lx, phys %lx\n",vma->vm_start,vma->vm_end - vma->vm_start,virt_to_phys(globalmem_devp->mem));
 }

 static void my_vma_close(struct vm_area_struct *vma)
 {
     printk(KERN_NOTICE "my VMA close.\n");
 }

 static struct vm_operations_struct my_remap_vm_ops = {
     .open = my_vma_open,
     .close = my_vma_close,
 };

static int globalmem_mmap(struct file *filp,struct vm_area_struct *vma)
{
     unsigned long page;  
     unsigned char i;  
     unsigned long start = (unsigned long)vma->vm_start;  
     //unsigned long end =  (unsigned long)vma->vm_end;  
     unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);  
   
     //得到物理地址  
     page = virt_to_phys(globalmem_devp->mem);
     printk(KERN_NOTICE "phys = %lx\n",page);      
     //将用户空间的一个vma虚拟内存区映射到以page开始的一段连续物理页面上  
     if(remap_pfn_range(vma,start,page>>PAGE_SHIFT,size,PAGE_SHARED))//第三个参数是页帧号，由物理地址右移PAGE_SHIFT得到  
         return -1;  
      
     vma->vm_ops = &my_remap_vm_ops;
     my_vma_open(vma); 

     //往该内存写10字节数据  
     for(i=0;i<10;i++)  
         globalmem_devp->mem[i] = array[i];  

    //  my_vma_close(vma);

     return 0;
}

static const struct file_operations globalmem_fops = {
	.owner = THIS_MODULE,
	.llseek = globalmem_llseek,
	.read = globalmem_read,
	.write = globalmem_write,
	.unlocked_ioctl = globalmem_ioctl,
	.open = globalmem_open,
	.release = globalmem_release,
	.mmap = globalmem_mmap,
};

static void __exit globalmem_exit(void)
{
	cdev_del(&globalmem_devp->cdev);
	kfree(globalmem_devp);
	unregister_chrdev_region(MKDEV(globalmem_major,0),1);
}
module_exit(globalmem_exit);

static void globalmem_setup_cdev(struct globalmem_dev* dev,int index)
{
	int err,devno = MKDEV(globalmem_major,index);

	cdev_init(&dev->cdev,&globalmem_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev,devno,1);
	if(err)
		printk(KERN_NOTICE "Error %d adding globalmem%d",err,index);
}

static int __init globalmem_init(void)
{
	int ret;
	dev_t devno = MKDEV(globalmem_major,0);
	
	if(globalmem_major)
		ret = register_chrdev_region(devno,1,"globalmem");
	else{
		ret = alloc_chrdev_region(&devno,0,1,"globalmem");
	}
	if(ret < 0)
		return ret;
	
	globalmem_devp = kzalloc(sizeof(struct globalmem_dev),GFP_KERNEL);
	if(!globalmem_devp){
		ret = -ENOMEM;
		goto fail_malloc;
	}
	
	mutex_init(&globalmem_devp->mutex);
	globalmem_setup_cdev(globalmem_devp,0);
	return 0;
	
	fail_malloc:
	unregister_chrdev_region(devno,1);
	return ret;
}
module_init(globalmem_init);

MODULE_AUTHOR("bo wang");
MODULE_LICENSE("GPL v2");
