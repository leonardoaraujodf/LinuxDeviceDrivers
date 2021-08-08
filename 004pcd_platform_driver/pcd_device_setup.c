#include <linux/module.h>
#include <linux/platform_device.h>

#include "platform.h"

#undef pr_fmt
#define pr_fmt(fmt) "[%s:%d] " fmt, __func__, __LINE__

void pcdev_release(struct device *dev);

struct pcdev_platform_data  pcdev_pdata[] = {
	[0] = {.size = 512, .perm = RDWR, .serial_number = "PCDEVABC1111"},
	[1] = {.size = 1024,.perm = RDWR, .serial_number = "PCDEVXYZ2222"},
};

struct platform_device platform_pcdev_1 = {
	.name = "pseudo-char-device",
	.id = 0,
	.dev = {
		.platform_data = &pcdev_pdata[0],
		.release = pcdev_release,
	},
};

struct platform_device platform_pcdev_2 = {
	.name = "pseudo-char-device",
	.id = 1,
	.dev = {
		.platform_data = &pcdev_pdata[1],
		.release = pcdev_release,
	},
};

void pcdev_release(struct device *dev)
{
	pr_info("Device released!");
	return;
}

static int __init pcdev_platform_init(void)
{
	platform_device_register(&platform_pcdev_1);
	platform_device_register(&platform_pcdev_2);

	pr_info("Device setup module loaded");

	return 0;
}

static void __exit pcdev_platform_exit(void)
{
	platform_device_unregister(&platform_pcdev_1);
	platform_device_unregister(&platform_pcdev_2);

	pr_info("Device setup module unloaded");
}

module_init(pcdev_platform_init);
module_exit(pcdev_platform_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Module which registers platform devices");
