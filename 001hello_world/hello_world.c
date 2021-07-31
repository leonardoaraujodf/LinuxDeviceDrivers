#include <linux/module.h>

static int __init hello_world_init(void)
{
   pr_info("Hello World!!!\n");
   return 0;
}

static void __exit hello_world_cleanup(void)
{
   pr_info("Good-bye World!!!\n");
}

module_init(hello_world_init);
module_exit(hello_world_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Leonardo Amorim");
MODULE_DESCRIPTION("A simple hello world kernel module.");
MODULE_INFO(board, "Beaglebone Black Rev. A5");
