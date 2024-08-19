#include "cp2102.h"
#include <linux/serial_reg.h>

#define BBB_UART_REGSHIFT   2
#define UART_BAUDRATE       115200

/*
 * Read/write function
 */

static u32 uart_read(struct cp2102_serial *serial, u32 reg)
{
    return readl_relaxed(serial->regs +  (reg << BBB_UART_REGSHIFT));
}

static void uart_write(struct cp2102_serial *serial, u32 reg, u32 val)
{
    while ( !(uart_read(serial, UART_LSR) & UART_LSR_THRE) ) {
        cpu_relax();
    }

    writel(val, serial->regs +  (reg << BBB_UART_REGSHIFT));
}

static void uart_str_write(struct cp2102_serial *serial, char *buf)
{
    while (*buf != '\0')
    {
        uart_write(serial, UART_TX, *buf);
        if (*buf == '\n')
            uart_write(serial, UART_TX, '\r');
        buf++;
    }
}

int circ_buf_isempty(struct cp2102_serial *serial)
{
    return serial->buf_head == serial->buf_tail;
}


/*
 * Interrupt request function
 */

static irqreturn_t cp2102_misc_isr(int irq, void *serial_id)
{
    int ret;
    unsigned long flags;
    struct cp2102_serial *serial = serial_id;

    if (uart_read(serial, UART_LSR) & UART_LSR_DR) {
        char msg = uart_read(serial, UART_RX);

        spin_lock_irqsave(&serial->lock, flags);

        serial->circ_buf[serial->buf_head] = msg;

        serial->buf_head++;
        if (serial->buf_head >= BUF_SIZE)
            serial->buf_head = 0;

        spin_unlock_irqrestore(&serial->lock, flags);

        wake_up(&serial->tty_wait);

        ret = IRQ_HANDLED;
    } else {
        pr_err("there is no new character in the RX FIFO.\n");
        ret = IRQ_NONE;
    }

    return ret;
}


static void bbb_uart_init(struct cp2102_serial *serial, unsigned int clock_freq)
{
    unsigned int baud_divisor = clock_freq / 16 / UART_BAUDRATE; 

    /* disable UART to config baudrate */ 
    uart_write(serial, UART_OMAP_MDR1, 0x7);
    
    /* swtich to register configuration mode B */ 
    uart_write(serial, UART_LCR, 0x00);
    uart_write(serial, UART_LCR, UART_LCR_DLAB);

    /* config baudrate */ 
    uart_write(serial, UART_DLL, (baud_divisor & 0xFF));
    uart_write(serial, UART_DLM, (baud_divisor >> 8) & 0xFF);

    /* set word length to 8 bit */ 
    uart_write(serial, UART_LCR, UART_LCR_WLEN8);

    /* enable UART 16x mode*/ 
    uart_write(serial, UART_OMAP_MDR1, 0x00);

    /* initialize interrupt */
    uart_write(serial, UART_IER, UART_IER_RDI);

    /* reset FIFO */
    uart_write(serial, UART_FCR, UART_FCR_CLEAR_XMIT | UART_FCR_CLEAR_RCVR);

    uart_str_write(serial, "Hello World, config done.!\n");
}

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

static ssize_t cp2102_misc_write(struct file *file, const char __user *buf,
                            size_t count, loff_t *offset)
{
    struct miscdevice *miscdev_ptr = file->private_data;
    struct platform_device *pdev = to_platform_device(miscdev_ptr->parent);
    struct cp2102_serial *serial = platform_get_drvdata(pdev);

    ssize_t len;

    /* get amount of data to copy */ 
    len = min(count, (size_t) (BUF_SIZE - *offset));

    /* copy data to user */ 
    if (copy_from_user(serial->tx_buffer + *offset, buf, len))
        return -EFAULT;

    uart_str_write(serial, serial->tx_buffer);

    *offset += len;
    return len;
}


static ssize_t cp2102_misc_read(struct file *file, char __user *buf,
                            size_t count, loff_t *offset)
{
    struct miscdevice *miscdev_ptr = file->private_data;
    struct platform_device *pdev = to_platform_device(miscdev_ptr->parent);
    struct cp2102_serial *serial = platform_get_drvdata(pdev);

    spin_lock_irq(&serial->tty_wait.lock); /* disable irq */ 

    wait_event_interruptible_locked_irq(serial->tty_wait, !circ_buf_isempty(serial));

    size_t len = min(count, (size_t) (BUF_SIZE - *offset));

    if (copy_to_user(buf, &serial->circ_buf[serial->buf_tail], len)) {
        return -EFAULT;
    }

    serial->buf_tail = (serial->buf_tail + (unsigned int) len) % BUF_SIZE;
    *offset += len;

    // Release lock
    spin_unlock_irq(&serial->tty_wait.lock); /* enable irq */ 

    return len;
}

/*
 * Misc Device Driver struct init
 */

static const struct file_operations fops = {
    .owner   =  THIS_MODULE,
    .write   =  cp2102_misc_write,
    .read    =  cp2102_misc_read,
    .open    =  cp2102_misc_open,
    .release =  cp2102_misc_close,
};

static struct platform_driver cp2102_platform_driver = {
    .driver    = {
        .name  = "cp2102",
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
    unsigned int clock_freq;
    
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

    /* initialize uart */ 
    ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency",
                                    &clock_freq);
    if (ret) {
        dev_err(&pdev->dev,
                    "clock-frequency property not found in Device Tree\n");
    }

    bbb_uart_init(serial, clock_freq);

    /* specific name for the device in devtmpfs */ 
    serial->miscdev.name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "cp2102-%x", serial->res->start);
    if (!serial->miscdev.name)
        return -ENOMEM;

    serial->miscdev.minor  = MISC_DYNAMIC_MINOR,
    serial->miscdev.fops   = &fops;
    serial->miscdev.parent = &pdev->dev;

    /* register a single misc device */ 
    ret = misc_register(&serial->miscdev);
    if (ret) {
        dev_err(&pdev->dev, "failed to register miscdev %d.\n", ret);
        return ret;
    }

    /* request interrupt service */ 
    serial->irq = platform_get_irq(pdev, 0);
    if (serial->irq < 0)
        return -ENXIO;
    pr_info("we get irq number: %d.\n", serial->irq);

    ret = devm_request_irq(&pdev->dev, serial->irq, cp2102_misc_isr,
                            0, pdev->name, (void *) serial); // no flags
    if (ret) {
        dev_err(&pdev->dev, "failed to request interrupt %d.\n", ret);
        misc_deregister(&serial->miscdev);
        return ret;
    }

    init_waitqueue_head(&serial->tty_wait);

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
