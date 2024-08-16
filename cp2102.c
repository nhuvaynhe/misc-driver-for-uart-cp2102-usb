#include "cp2102.h"

/*
 * File operations function
 */

static int cp2102_misc_open(struct inode *inode, struct file *file)
{
    pr_info("CP2102 misc device open\n");
    return 0;
}

static int cp2102_misc_close(struct inode *inode, struct file *file)
{
    pr_info("CP2102 misc device close\n");
    return 0;
}

static int cp2102_misc_write(struct file *f, const char __user *buf,
                            size_t len, loff_t *ppos)
{
    pr_info("CP2102 misc device write\n");
    return len;
}

/*
 * Misc Device Driver struct init
 */

static int cp2102_misc_read(struct file *f, char __user *buf,
                            size_t len, loff_t *ppos)
{
    pr_info("CP2102 misc device read\n");
    return 0;
}


static const struct file_operations fops = {
    .owner   =  THIS_MODULE,
    .write   =  cp2102_misc_write,
    .read    =  cp2102_misc_read,
    .open    =  cp2102_misc_open,
    .release =  cp2102_misc_close,
};

static struct platform_driver cp2102_platform_driver = {
    .driver    = {
        .name  = "cp2102_platform_device",
        .of_match_table = of_uart_platform_device_match,
        .owner = THIS_MODULE,
    },
    .probe     = cp2102_platform_device_probe,
    .remove    = cp2102_platform_device_remove,
};

/*
 * Misc Probe function
 */

static int cp2102_platform_device_probe(struct platform_device *pdev)
{
    pr_info("cp2102_platform_device_probe.\n");
    int ret;
    
    struct cp2102_serial *serial;
    serial = devm_kzalloc(&pdev->dev, sizeof(*serial), GFP_KERNEL);
    if (!serial) {
        dev_err(&pdev->dev, "alloc failed. \n");
        return -EINVAL;
    }

    /* power management initialization */ 
    pm_runtime_enable(&pdev->dev);
    pm_runtime_get_sync(&pdev->dev);

    platform_set_drvdata(pdev, serial);

    /* restrive device physiscal address from device tree */ 
    serial->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!serial->res) {
        dev_err(&pdev->dev, "missing register.\n");
        return -EINVAL;
    }

    /* get a base virutal address for device register */ 
    serial->regs = devm_platform_ioremap_resource(pdev, 0);
    if (IS_ERR(serial->regs))
        return PTR_ERR(serial->regs);

    /* specific name for the device in devtmpfs */ 
    serial->miscdev.name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "cp2102-%x", serial->res->start);
    if (!serial->miscdev.name)
        return -ENOMEM;

    serial->miscdev.minor  = MISC_DYNAMIC_MINOR,
    serial->miscdev.fops = &fops;
    serial->miscdev.parent = &pdev->dev;

    /* register a single misc device */ 
    ret = misc_register(&serial->miscdev);
    if (ret) {
        dev_err(&pdev->dev, "failed to register miscdev %d.\n", ret);
        return ret;
    }
        
    return 0;
}

static int cp2102_platform_device_remove(struct platform_device *pdev)
{
    pr_info("cp2102_platform_device_remove.\n");

    struct cp2102_serial *serial = platform_get_drvdata(pdev);

    pm_runtime_disable(&pdev->dev);
    misc_deregister(&serial->miscdev);

    return 0;
}

/*
 * Misc Init function
 */
static int __init cp2102_misc_device_init(void)
{
    pr_info("cp2102_misc_device_init.\n");
    return platform_driver_register(&cp2102_platform_driver);
}

/*
 * Misc Exit function
 */
static void __exit cp2102_misc_device_exit(void)
{
    pr_info("cp2102_misc_device_exit.\n");
    platform_driver_unregister(&cp2102_platform_driver);
}

module_init(cp2102_misc_device_init);
module_exit(cp2102_misc_device_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple misc driver for CP2102 UART TTY USB");
MODULE_AUTHOR("Ngoc Dai");
