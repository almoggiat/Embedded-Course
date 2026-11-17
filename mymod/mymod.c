#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/fs.h>

static dev_t dev_num;

static int __init mymod_init(void) {
    int ret;

    ret = alloc_chrdev_region(&dev_num, 0, 1, "mymod");

    if (ret < 0) {
        printk(KERN_ERR "Failed to allocate device number\n");
        return ret;
    }

    printk(KERN_INFO "module loaded\n");
    printk(KERN_INFO "Major: %d, Minor: %d\n",
           MAJOR(dev_num), MINOR(dev_num));

    return 0;
}

static void __exit mymod_exit(void) {
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "module unloaded\n");
}

module_init(mymod_init);
module_exit(mymod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("My character device module");
