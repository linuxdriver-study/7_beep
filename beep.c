#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/ide.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#define BEEPDEV_CNT             1
#define BEEPDEV_NAME            "beep"
#define BEEP_ON                 1
#define BEEP_OFF                0

struct beep_device {
        dev_t devid;
        int major;
        int minor;
        struct cdev c_dev;
        struct class *class;
        struct device *device;
        struct device_node *nd;
        int beep_gpio;
};
static struct beep_device beep_dev;

static int beep_open(struct inode *nd, struct file *file);
static ssize_t beep_write(struct file *file,
                          const char __user *user,
                          size_t size,
                          loff_t *loff);
static int beep_release(struct inode *nd, struct file *file);

static struct file_operations beep_ops = {
        .owner = THIS_MODULE,
        .open = beep_open,
        .write = beep_write,
        .release = beep_release,
};

static int beep_open(struct inode *nd, struct file *file)
{
        file->private_data = &beep_dev;
        return 0;
}

static ssize_t beep_write(struct file *file,
                          const char __user *user,
                          size_t size,
                          loff_t *loff)
{
        unsigned char buf[1] = {0};
        struct beep_device *dev = file->private_data;
        int ret = 0;

        ret = copy_from_user(buf, user, 1);
        if (ret != 0) {
                ret = -EINVAL;
                goto error;
        }
        if ((buf[0] != BEEP_OFF) && (buf[0] != BEEP_ON)) {
                ret = -EINVAL;
                goto error;
        }

        if (buf[0] == BEEP_OFF) {
                gpio_set_value(dev->beep_gpio, 1);
        } else if (buf[0] == BEEP_ON) {
                gpio_set_value(dev->beep_gpio, 0);
        }

error:
        return ret;
}

static int beep_release(struct inode *nd, struct file *file)
{
        file->private_data = NULL;
        return 0;
}

static int __init beep_init(void)
{
        int ret = 0;

        if (beep_dev.major == 0) {
                ret = alloc_chrdev_region(&beep_dev.devid, 0,
                                        BEEPDEV_CNT, BEEPDEV_NAME);
        } else {
                beep_dev.devid = MKDEV(beep_dev.major, 0);
                ret = register_chrdev_region(beep_dev.devid, 
                                        BEEPDEV_CNT, BEEPDEV_NAME);
        }
        if (ret < 0) {
                printk("chrdev region error!\n");
                goto fail_chrdev_region;
        }
        beep_dev.major = MAJOR(beep_dev.devid);
        beep_dev.minor = MINOR(beep_dev.devid);
        printk("major:%d minor:%d\n", beep_dev.major, beep_dev.minor);

        beep_dev.c_dev.owner = THIS_MODULE;
        cdev_init(&beep_dev.c_dev, &beep_ops);
        ret = cdev_add(&beep_dev.c_dev, beep_dev.devid, BEEPDEV_CNT);
        if (ret < 0) {
                printk("cdev add error!\n");
                goto fail_cdevadd;
        }

        beep_dev.class = class_create(THIS_MODULE, BEEPDEV_NAME);
        if (IS_ERR(beep_dev.class)) {
                ret = PTR_ERR(beep_dev.class);
                printk("class create error!\n");
                goto fail_class_create;
        }
        beep_dev.device = device_create(beep_dev.class, NULL, 
                                        beep_dev.devid, NULL, 
                                        BEEPDEV_NAME);
        if (IS_ERR(beep_dev.device)) {
                ret = PTR_ERR(beep_dev.device);
                printk("device create error!\n");
                goto fail_device_create;
        }

        beep_dev.nd = of_find_node_by_path("/beep");
        if (beep_dev.nd == NULL) {
                printk("find node error");
                ret = -EINVAL;
                goto fail_find_node;
        }
        beep_dev.beep_gpio = of_get_named_gpio(beep_dev.nd, "beep-gpios", 0);
        if (beep_dev.beep_gpio < 0) {
                printk("get named error!\n");
                ret = -EFAULT;
                goto fail_get_named;
        }
        printk("beep-gpio num = %d\n", beep_dev.beep_gpio);
        ret = gpio_request(beep_dev.beep_gpio, "beep");
        if (ret != 0) {
                printk("gpio request error!\n");
                goto fail_gpio_request;
        }
        ret = gpio_direction_output(beep_dev.beep_gpio, 1);
        if (ret != 0) {
                printk("gpio dir output set error!\n");
                goto fail_dir_error;
        }
        gpio_set_value(beep_dev.beep_gpio, 1);  /* default close beep */

        goto success;
        
fail_dir_error:
        gpio_free(beep_dev.beep_gpio);
fail_gpio_request:
fail_get_named:
fail_find_node:
        device_destroy(beep_dev.class, beep_dev.devid);
fail_device_create:
        class_destroy(beep_dev.class);
fail_class_create:
        cdev_del(&beep_dev.c_dev);
fail_cdevadd:
        unregister_chrdev_region(beep_dev.devid, BEEPDEV_CNT);
fail_chrdev_region:
success:
        return ret;
}

static void __exit beep_exit(void)
{
        gpio_set_value(beep_dev.beep_gpio, 1);
        gpio_free(beep_dev.beep_gpio);
        device_destroy(beep_dev.class, beep_dev.devid);
        class_destroy(beep_dev.class);
        cdev_del(&beep_dev.c_dev);
        unregister_chrdev_region(beep_dev.devid, BEEPDEV_CNT);
}

module_init(beep_init);
module_exit(beep_exit);
MODULE_AUTHOR("wanglei");
MODULE_LICENSE("GPL");
