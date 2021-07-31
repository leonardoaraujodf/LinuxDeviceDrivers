#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>

#undef pr_fmt
#define pr_fmt(fmt) "[%s:%d] "fmt, __func__, __LINE__

#define NO_OF_DEVICES ( 4 )

#define RDONLY ( 0x01 )
#define WRONLY ( 0x10 )
#define RDWR   ( 0x11 )

#define MEM_SIZE_MAX_PCDEV1 ( 1024 )
#define MEM_SIZE_MAX_PCDEV2 ( 1024 )
#define MEM_SIZE_MAX_PCDEV3 ( 1024 )
#define MEM_SIZE_MAX_PCDEV4 ( 1024 )

/* pseudo device's memory */
static char device_buffer_pcdev1[MEM_SIZE_MAX_PCDEV1];
static char device_buffer_pcdev2[MEM_SIZE_MAX_PCDEV2];
static char device_buffer_pcdev3[MEM_SIZE_MAX_PCDEV3];
static char device_buffer_pcdev4[MEM_SIZE_MAX_PCDEV4];

/* Device private data structure */
struct pcdev_private_data {
	char *buffer;
	unsigned int size;
	const char *serial_number;
	int perm;
	struct cdev cdev;
};

/* Driver private data structure */
struct pcdrv_private_data {
	int total_devices;
	/* this holds the device number */
	dev_t pcd_dev_number;
	struct class * pcd_class;
	struct device * pcd_dev;
	struct pcdev_private_data pcdev_data[NO_OF_DEVICES];
};

static struct pcdrv_private_data pcdrv_data = {
	.total_devices = NO_OF_DEVICES,
	.pcdev_data = {
		[0] = {
			.buffer = device_buffer_pcdev1,
			.size = MEM_SIZE_MAX_PCDEV1,
			.serial_number = "PCDEV1XYZ123",
			.perm = RDONLY, /* RDONLY */
		},
		[1] = {
			.buffer = device_buffer_pcdev2,
			.size = MEM_SIZE_MAX_PCDEV2,
			.serial_number = "PCDEV2XYZ123",
			.perm = WRONLY, /* WRONLY */
		},
		[2] = {
			.buffer = device_buffer_pcdev3,
			.size = MEM_SIZE_MAX_PCDEV3,
			.serial_number = "PCDEV3XYZ123",
			.perm = RDWR, /* RDWR */
		},
		[3] = {
			.buffer = device_buffer_pcdev4,
			.size = MEM_SIZE_MAX_PCDEV4,
			.serial_number = "PCDEV4XYZ123",
			.perm = RDONLY, /* RDONLY */
		},
	}
};


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


loff_t pcd_lseek(struct file *filp, loff_t off, int whence)
{
	loff_t temp;
	struct pcdev_private_data *priv = (struct pcdev_private_data *)filp->private_data;
	int max_size = priv->size;
	pr_info("lseek requested\n");
	pr_info("Current file position = %lld\n", filp->f_pos);

	switch(whence)
	{
		case SEEK_SET:
			if( (off > max_size) || (off < 0))
				return -EINVAL;
			filp->f_pos = off;
			break;
		case SEEK_CUR:
			temp = filp->f_pos + off;
			if ((temp > max_size) || (temp < 0))
				return -EINVAL;
			filp->f_pos = temp;
			break;
		case SEEK_END:
			temp = max_size + off;
			if ((temp > max_size) || (temp < 0))
				return -EINVAL;
			filp->f_pos = max_size + off;
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
	struct pcdev_private_data *priv = (struct pcdev_private_data *)filp->private_data;
	int max_size = priv->size;

	pr_info("Read requested for %zu bytes\n", count);
	pr_info("Current file position = %lld\n", *f_pos);

	/* Adjust the 'count' */
	if ((*f_pos + count) > max_size)
		count = max_size - *f_pos;

	/* copy to user */
	if (copy_to_user(buff, &priv->buffer[*f_pos], count)) {
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
	struct pcdev_private_data *priv = (struct pcdev_private_data *)filp->private_data;
	int max_size = priv->size;

	pr_info("Write requested for %zu bytes \n", count);
	pr_info("Current file position = %lld\n", *f_pos);

	/* Adjust the 'count' */
	if (((*f_pos) + count) > max_size)
		count = max_size - (*f_pos);

	if(!count)
	{
		pr_err("No space left on the device!\n");
		return -ENOMEM;
	}

	/* copy from user */
	if (copy_from_user(&priv->buffer[*f_pos], buff, count)) {
		return -EFAULT;
	}

	/* update the current file position */
	*f_pos += count;
	pr_info("Number of bytes written successfully = %zu\n", count);
	pr_info("Updated file position = %lld\n", *f_pos);

	/* return the number of bytes which have been succesfully writen */
	return count;
}


int check_permission(int dev_perm, int acc_mode)
{
	if(dev_perm == RDWR)
		return 0;
	else if((dev_perm == RDONLY) && (acc_mode & FMODE_READ)
		&& !(acc_mode & FMODE_WRITE))
		return 0;
	else if((dev_perm == WRONLY) && (acc_mode & FMODE_WRITE)
		&& !(acc_mode & FMODE_READ))
		return 0;

	return -EPERM;
}

int pcd_open(struct inode *inode, struct file *filp)
{
	int ret;
	int minor_n;
	struct pcdev_private_data *priv;

	/* find out on which device file open was attempted by the user space */
	minor_n = MINOR(inode->i_rdev);
	pr_info("minor access = %d\n", minor_n);

	/* get device's private data structure */
	priv = container_of(inode->i_cdev, struct pcdev_private_data, cdev);
	/* supply device private data to other methods of the driver */
	filp->private_data = priv;

	/* check permission */
	ret = check_permission(priv->perm, filp->f_mode);
	if(ret)
		pr_info("Open unsuccesful\n");
	else
		pr_info("Open was succesful\n");

	return ret;
}

int pcd_release(struct inode *inode, struct file *filp)
{
	pr_info("Release was successful\n");
	return 0;
}

static int __init pcd_driver_init(void)
{
	int ret;
	int i;
	struct pcdrv_private_data *p = &pcdrv_data;

	/* 1. Dynamically allocate a device number */
	ret = alloc_chrdev_region(&p->pcd_dev_number, 0, NO_OF_DEVICES,
			          "pcd_devices");
	if (ret < 0) {
		pr_err("alloc_chrdev_region failed\n");
		goto out;
	}

	/* 2. Create device class under /sys/class/ */
	p->pcd_class = class_create(THIS_MODULE, "pcd_class");
	if (IS_ERR(p->pcd_class)) {
		pr_err("class_create failed!\n");
		/*
		* PTR_ERR() converts pointer to error code (int)
		* ERR_PTR() converts error code (int) to pointer
		*/
		ret = PTR_ERR(p->pcd_class);
		goto unreg_chrdev;
	}

	for(i = 0; i < NO_OF_DEVICES; i++) {
		pr_info("Device number <major><minor> = %d:%d\n",
			MAJOR(p->pcd_dev_number + i),
			MINOR(p->pcd_dev_number + i));

		/* 3. Initialize the cdev struct with fops */
		cdev_init(&p->pcdev_data[i].cdev, &pcd_fops);

		/* 4. Register a device (cdev) structure with VFS. */
		p->pcdev_data[i].cdev.owner = THIS_MODULE;
		ret = cdev_add(&p->pcdev_data[i].cdev,
			       p->pcd_dev_number + i, 1);
		if (ret < 0) {
			pr_err("cdev_add failed!\n");
			goto cdev_del;
		}

		/* 5. Populate the sysfs with device information */
		p->pcd_dev = device_create(p->pcd_class, NULL,
				           p->pcd_dev_number + i, NULL,
					   "pcdev-%d", i + 1);
		if (IS_ERR(p->pcd_dev)) {
			pr_err("device_create failed!\n");
			ret = PTR_ERR(p->pcd_dev);
			goto cdev_del;
		}
	}

	pr_info("Module init was succesful\n");

	return 0;

cdev_del:
	for(;i >= 0; i--){
		device_destroy(p->pcd_class, p->pcd_dev_number + i);
		cdev_del(&p->pcdev_data[i].cdev);
	}
	class_destroy(p->pcd_class);
unreg_chrdev:
	unregister_chrdev_region(p->pcd_dev_number, NO_OF_DEVICES);
out:
	pr_info("Module insertion failed\n");
	return ret;
}

static void __exit pcd_driver_cleanup(void)
{
	int i;
	struct pcdrv_private_data *p = &pcdrv_data;

	for(i = 0;i < NO_OF_DEVICES; i++){
		device_destroy(p->pcd_class, p->pcd_dev_number + i);
		cdev_del(&p->pcdev_data[i].cdev);
	}
	class_destroy(p->pcd_class);
	unregister_chrdev_region(p->pcd_dev_number, NO_OF_DEVICES);
	pr_info("Module unloaded\n");
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Leonardo");
MODULE_DESCRIPTION("A pseudo character driver which handles n devices");
