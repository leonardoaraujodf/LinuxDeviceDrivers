#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>

#undef pr_fmt
#define pr_fmt(fmt) "[%s:%d] "fmt, __func__, __LINE__

#define DEV_MEM_SIZE (512)
/* pseudo device's memory */
static char device_buffer[DEV_MEM_SIZE];

/* this holds the device number */
static dev_t pcd_dev_number;

/* cdev variable */
static struct cdev pcd_cdev;

loff_t pcd_lseek(struct file *filp, loff_t off, int whence);
ssize_t pcd_read(struct file *filp, char __user *buff, size_t count,
		 loff_t *f_pos);
ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count,
		  loff_t *f_pos);
int pcd_open(struct inode *inode, struct file *filp);
int pcd_release(struct inode *inode, struct file *filp);

/* file operations of the driver */
static struct file_operations pcd_fops = {
	.llseek = pcd_lseek,
	.read = pcd_read,
	.write = pcd_write,
	.open = pcd_open,
	.release = pcd_release,
	.owner = THIS_MODULE
};

struct class * pcd_class;
struct device * pcd_dev;

loff_t pcd_lseek(struct file *filp, loff_t off, int whence)
{
   loff_t temp;
   pr_info("lseek requested\n");
   pr_info("Current file position = %lld\n", filp->f_pos);

   switch(whence)
   {
      case SEEK_SET:
         if( (off > DEV_MEM_SIZE) || (off < 0))
            return -EINVAL;
         filp->f_pos = off;
         break;
      case SEEK_CUR:
         temp = filp->f_pos + off;
         if ((temp > DEV_MEM_SIZE) || (temp < 0))
            return -EINVAL;
         filp->f_pos = temp;
         break;
      case SEEK_END:
         temp = DEV_MEM_SIZE + off;
         if ((temp > DEV_MEM_SIZE) || (temp < 0))
            return -EINVAL;
         filp->f_pos = DEV_MEM_SIZE + off;
         break;
      default:
         return -EINVAL;
   }

   pr_info("New value of file pointer = %lld\n", filp->f_pos);
   return filp->f_pos;
}

ssize_t pcd_read(struct file *filp, char __user *buff, size_t count,
      loff_t *f_pos)
{
	pr_info("Read requested for %zu bytes\n", count);
	pr_info("Current file position = %lld\n", *f_pos);

	/* Adjust the 'count' */
	if ((*f_pos + count) > DEV_MEM_SIZE)
		count = DEV_MEM_SIZE - *f_pos;

	/* copy to user */
	if (copy_to_user(buff, &device_buffer[*f_pos], count)) {
		return -EFAULT;
	}

	/* update the current file position */
	*f_pos += count;
	pr_info("Number of bytes succesfully read = %zu\n", count);
	pr_info("Updated file position = %lld\n", *f_pos);

	/* return the number of bytes which have been succesfully read */
	return count;
}

ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count,
      loff_t *f_pos)
{
	pr_info("Write requested for %zu bytes \n", count);
	pr_info("Current file position = %lld\n", *f_pos);

	/* Adjust the 'count' */
	if ((*f_pos + count) > DEV_MEM_SIZE)
		count = DEV_MEM_SIZE - *f_pos;

	if(!count)
	{
		pr_err("No space left on the device!\n");
		return -ENOMEM;
	}

	/* copy from user */
	if (copy_from_user(&device_buffer[*f_pos], buff, count)) {
		return -EFAULT;
	}

	/* update the current file position */
	*f_pos += count;
	pr_info("Number of bytes written successfully = %zu\n", count);
	pr_info("Updated file position = %lld\n", *f_pos);

	/* return the number of bytes which have been succesfully writen */
	return count;
}

int pcd_open(struct inode *inode, struct file *filp)
{
	pr_info("Open was succesful\n");
	return 0;
}

int pcd_release(struct inode *inode, struct file *filp)
{
	pr_info("Release was successful\n");
	return 0;
}

static int __init pcd_driver_init(void)
{
	int ret;
	/* 1. Dynamically allocate a device number */
	ret = alloc_chrdev_region(&pcd_dev_number, 0, 1, "pcd_devices" );
	if (ret < 0) {
		pr_err("alloc_chrdev_region failed\n");
		goto out;
	}

	pr_info("Device number <major><minor> = %d:%d\n",
	MAJOR(pcd_dev_number), MINOR(pcd_dev_number));

	/* 2. Initialize the cdev struct with fops */
	cdev_init(&pcd_cdev, &pcd_fops);

	/* 3. Register a device (cdev) structure with VFS. */
	pcd_cdev.owner = THIS_MODULE;
	ret = cdev_add(&pcd_cdev, pcd_dev_number, 1);
	if (ret < 0) {
		pr_err("cdev_add failed!\n");
		goto unreg_chrdev;
	}

	/* 4. Create device class under /sys/class/ */
	pcd_class = class_create(THIS_MODULE, "pcd_class");
	if (IS_ERR(pcd_class)) {
		pr_err("class_create failed!\n");
		/*
		* PTR_ERR() converts pointer to error code (int)
		* ERR_PTR() converts error code (int) to pointer
		*/
		ret = PTR_ERR(pcd_class);
		goto cdev_del;
	}

	/* 5. Populate the sysfs with device information */
	pcd_dev = device_create(pcd_class, NULL, pcd_dev_number, NULL, "pcd");
	if (IS_ERR(pcd_dev)) {
		pr_err("device_create failed!\n");
		ret = PTR_ERR(pcd_dev);
		goto class_del;
	}

	pr_info("Module init was succesful\n");

	return 0;

class_del:
	class_destroy(pcd_class);
cdev_del:
	cdev_del(&pcd_cdev);
unreg_chrdev:
	unregister_chrdev_region(pcd_dev_number, 1);
out:
	return ret;
}

static void __exit pcd_driver_cleanup(void)
{
	device_destroy(pcd_class, pcd_dev_number);
	class_destroy(pcd_class);
	cdev_del(&pcd_cdev);
	unregister_chrdev_region(pcd_dev_number, 1);
	pr_info("Module unloaded\n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Leonardo");
MODULE_DESCRIPTION("A pseudo character driver");
