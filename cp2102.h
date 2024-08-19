#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/pm_runtime.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/serial_reg.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#define BUF_SIZE    16

struct cp2102_serial {
    void __iomem *regs;
    struct miscdevice miscdev;
    struct device *dev;
    struct resource *res;
    int irq;
    char circ_buf[BUF_SIZE];
    char tx_buffer[BUF_SIZE];
    unsigned int buf_tail;
    unsigned int buf_head;
    spinlock_t lock;
    wait_queue_head_t tty_wait;
};

static const struct of_device_id of_uart_platform_device_match[] = {
    { .compatible = "bbb,uart2", },
    { .compatible = "bbb,uart4", },
    { }
};

static int cp2102_platform_device_probe(struct platform_device *pdev);
static int cp2102_platform_device_remove(struct platform_device *pdev);
static irqreturn_t cp2102_misc_isr(int irq, void *serial);
