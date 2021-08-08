#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include "platform.h"

#undef pr_fmt
#define pr_fmt(fmt) "[%s:%d] " fmt, __func__, __LINE__

/* Maximum number of devices this driver supports. */
#define MAX_DEVICES (10)

int pcd_open(struct inode *inode, struct file *filp);
int pcd_release(struct inode *inode, struct file *flip);
ssize_t pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos);
ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos);
loff_t pcd_lseek(struct file *filp, loff_t offset, int whence);

int pcd_platform_driver_probe(struct platform_device *pdev);
int pcd_platform_driver_remove(struct platform_device *pdev);

/* Device private data structure */
struct pcdev_private_data {
	struct pcdev_platform_data pdata;
	char *buffer;
	dev_t dev_num;
	struct cdev cdev;
};

/* Driver private data structure */
struct pcdrv_private_data {
	int total_devices;
	dev_t device_num_base;
	struct class * class_pcd;
	struct device * device_pcd;
};

/* file operations of the driver */
struct file_operations pcd_fops =
{
	.open = pcd_open,
	.release = pcd_release,
	.read = pcd_read,
	.write = pcd_write,
	.llseek = pcd_lseek,
	.owner = THIS_MODULE
};

struct platform_driver pcd_platform_driver =
{
	.probe = pcd_platform_driver_probe,
	.remove = pcd_platform_driver_remove,
	.driver = {
		.name = "pseudo-char-device"
	}
};

/* Driver private data structure */
static struct pcdrv_private_data pcdrv_private_data;

static int check_permission(int dev_perm, int acc_mode)
{
	if (dev_perm == RDWR)
		return 0;
	else if ((dev_perm == RDONLY) && (acc_mode & FMODE_READ)
		&& !(acc_mode & FMODE_WRITE))
		return 0;
	else if ((dev_perm == WRONLY) && (acc_mode & FMODE_WRITE)
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
	pr_info("Minor access = %d\n", minor_n);

	/* gets device's private data structure */
	priv = container_of(inode->i_cdev, struct pcdev_private_data, cdev);
	/* supply device private data to other methods of the driver */
	filp->private_data = priv;

	/* check permission */
	ret = check_permission(priv->pdata.perm, filp->f_mode);
	if (ret)
		pr_info("Open unsuccesful\n");
	else
		pr_info("Open was successful\n");

	return ret;
}

int pcd_release(struct inode *inode, struct file *flip)
{
	pr_info("Release was succesful\n");
	return 0;
}

ssize_t pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos)
{
	struct pcdev_private_data *priv = (struct pcdev_private_data *)filp->private_data;
	int max_size = priv->pdata.size;

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

ssize_t pcd_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos)
{
	struct pcdev_private_data *priv = (struct pcdev_private_data *)filp->private_data;
	int max_size = priv->pdata.size;

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

loff_t pcd_lseek(struct file *filp, loff_t off, int whence)
{
	loff_t temp;
	struct pcdev_private_data *priv = (struct pcdev_private_data *)filp->private_data;
	int max_size = priv->pdata.size;

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

/* Get's called when matched platform device is found */
int pcd_platform_driver_probe(struct platform_device *pdev)
{
	int ret;
	struct pcdev_private_data *dev_priv;
	struct pcdev_platform_data *dev_plat;
	struct pcdrv_private_data *drv_priv = &pcdrv_private_data;

	pr_info("A device is detected!\n");

	/* 1. Get the platform data */
	dev_plat = (struct pcdev_platform_data *)dev_get_platdata(&pdev->dev);
	if (!dev_plat)
	{
		pr_err("No platform data available!\n");
		ret = -EINVAL;
		goto out;
	}

	/* 2. Dynamically allocate data for the device private data */
	dev_priv = devm_kzalloc(&pdev->dev, sizeof(*dev_priv), GFP_KERNEL);
	if (!dev_priv)
	{
		pr_info("Cannot allocate memory!\n");
		ret = -ENOMEM;
		goto out;
	}

	/* Save device data in dev structure so it could be removed in remove
	 * function */
	dev_set_drvdata(&pdev->dev, dev_priv);

	memcpy(&dev_priv->pdata, dev_plat, sizeof(*dev_plat));
	pr_info("Device serial number: %s\n", dev_priv->pdata.serial_number);
	pr_info("Device permission: 0x%X\n", dev_priv->pdata.perm);
	pr_info("Device size: %u\n", dev_priv->pdata.size);

	/* 3. Dynamically allocate data for the device buffer using
	 * size information from the platform data. */
	dev_priv->buffer = devm_kzalloc(&pdev->dev, dev_priv->pdata.size,
					GFP_KERNEL);
	if (!dev_priv)
	{
		pr_info("Cannot allocate memory!\n");
		ret = -ENOMEM;
		goto free_dev_priv;
	}

	/* 4. Get the device number */
	dev_priv->dev_num = drv_priv->device_num_base + pdev->id;

	/* 5. Do cdev init and cdev add */
	cdev_init(&dev_priv->cdev, &pcd_fops);
	dev_priv->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev_priv->cdev, dev_priv->dev_num, 1);
	if (ret < 0) {
		pr_err("cdev_add failed!\n");
		goto free_buff;
	}

	/* 6. Create device file for the detected platform device */
	drv_priv->device_pcd = device_create(drv_priv->class_pcd, NULL,
			                     dev_priv->dev_num, NULL,
				             "pcdev-%d", pdev->id);
	if (IS_ERR(drv_priv->device_pcd)) {
		pr_err("device_create failed!\n");
		ret = PTR_ERR(drv_priv->device_pcd);
		goto cdev_del;
	}

	/* 7. Error handling */
	drv_priv->total_devices++;
	pr_info("Probe was successful!\n");
	return 0;

cdev_del:
	cdev_del(&dev_priv->cdev);
free_buff:
	devm_kfree(&pdev->dev, dev_priv->buffer);
free_dev_priv:
	devm_kfree(&pdev->dev, dev_priv);
out:
	pr_err("Device probe failed\n");
	return ret;
}

int pcd_platform_driver_remove(struct platform_device *pdev)
{
	struct pcdev_private_data *dev_priv = dev_get_drvdata(&pdev->dev);

	pr_info("A device is being removed\n");
	/* 1. Remove a device that was created with device_create() */
	device_destroy(pcdrv_private_data.class_pcd, dev_priv->dev_num);
	/* 2. Remove a cdev entry from the system */
	cdev_del(&dev_priv->cdev);

	pcdrv_private_data.total_devices--;
	pr_info("Device removed!\n");
	return 0;
}

static int __init pcd_platform_driver_init(void)
{
	int ret;
	struct pcdrv_private_data *priv = &pcdrv_private_data;
	/* 1. Dynamically allocate a device number for MAX_DEVICES. */
	ret = alloc_chrdev_region(&priv->device_num_base, 0, MAX_DEVICES,
			          "pcdevs");
	if (ret < 0)
	{
		pr_err("alloc_chrdev_region failed!\n");
		return ret;
	}
	/* 2. Create device class under /sys/class */
	priv->class_pcd = class_create(THIS_MODULE, "pcd_class");
	if (IS_ERR(priv->class_pcd)) {
		pr_err("class_create failed!\n");
		/*
		* PTR_ERR() converts pointer to error code (int)
		* ERR_PTR() converts error code (int) to pointer
		*/
		ret = PTR_ERR(priv->class_pcd);
		goto unreg_chrdev;
	}
	/* 3. Register a platform driver */
	platform_driver_register(&pcd_platform_driver);
	pr_info("Platform driver loaded\n");
	return 0;

unreg_chrdev:
	unregister_chrdev_region(priv->device_num_base, MAX_DEVICES);
	return ret;
}

static void __exit pcd_platform_driver_cleanup(void)
{
	struct pcdrv_private_data *priv = &pcdrv_private_data;
	/* 1. Unregister the platform driver */
	platform_driver_unregister(&pcd_platform_driver);
	/* 2. Class destroy */
	class_destroy(priv->class_pcd);
	/* 3. Unregister device numbers for MAX_DEVICES */
	unregister_chrdev_region(priv->device_num_base, MAX_DEVICES);
	pr_info("Platform driver unloaded\n");
	return;
}

module_init(pcd_platform_driver_init);
module_exit(pcd_platform_driver_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Leonardo Amorim");
MODULE_DESCRIPTION("A pseudo character platform driver which handles n platform pcdevs");
