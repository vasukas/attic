// Based on http://derekmolloy.ie/writing-a-linux-kernel-module-part-1-introduction/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

//
// Module parameters
//

static char *testpar = "NoValue"; // parameter which can be passed with 'insmod'
module_param(testpar, charp, S_IRUGO);
MODULE_PARM_DESC(testpar, "a parameter");

static int dev_number = -1;
static struct class* dev_class = NULL;
static struct device* dev_device = NULL;

//
// Device
//

#define DEVICE_NAME "uwcajoychar"
#define CLASS_NAME  "uwcajoy"

// TODO: thread safety?

static int test_counter = 0;

static int dev_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "uwcajoy: dev_open called\n");
    return 0;
}
static ssize_t dev_read(struct file *file, char *buffer, size_t len, loff_t *offset) {
    uint8_t b = ++test_counter;
    copy_to_user(buffer, &b, 1);
    return 1;
}
static ssize_t dev_write(struct file *file, const char *buffer, size_t len, loff_t *offset) {
    // TODO: don't implement this?
    return 0;
}
static int dev_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "uwcajoy: dev_release called\n");
    return 0;
}

// see linux/fs.h
static struct file_operations fops = {
    // TODO: add ioctl handler
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

//
// Module
//

MODULE_LICENSE("GPL"); // for fuck's sake
MODULE_AUTHOR("vasukas");
MODULE_DESCRIPTION("example driver");
MODULE_VERSION("0.0.1");

static int __init uwcajoy_init(void) {
    printk(KERN_INFO "uwcajoy: init (testpar = %s)\n", testpar);
    
    dev_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (dev_number < 0){
        printk(KERN_ALERT "uwcajoy: register_chrdev failed (%d)\n", dev_number);
        return dev_number;
    }
    printk(KERN_INFO "uwcajoy: register_chrdev ok: %d\n", dev_number);
    
    dev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(dev_class)) {
        unregister_chrdev(dev_number, DEVICE_NAME);
        printk(KERN_ALERT "uwcajoy: class_create failed\n");
        return PTR_ERR(dev_class);
    }
    printk(KERN_INFO "uwcajoy: class_create ok\n");
    
    dev_device = device_create(dev_class, NULL, MKDEV(dev_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(dev_device)) {
        class_destroy(dev_class);
        unregister_chrdev(dev_number, DEVICE_NAME);
        printk(KERN_ALERT "uwcajoy: device_create failed\n");
        return PTR_ERR(dev_device);
    }
    printk(KERN_INFO "uwcajoy: device_create ok\n");

    return 0; // return negative on error
}
static void __exit uwcajoy_exit(void) {
    printk(KERN_INFO "uwcajoy: exiting...\n");
    
    device_destroy(dev_class, MKDEV(dev_number, 0));
    class_unregister(dev_class);
    class_destroy(dev_class);
    unregister_chrdev(dev_number, DEVICE_NAME);
    
    printk(KERN_INFO "uwcajoy: exit\n");
}

module_init(uwcajoy_init);
module_exit(uwcajoy_exit);

