#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/pm_runtime.h>
#include <linux/fs.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

struct cp2102_serial {
    void __iomem *regs;
    struct miscdevice miscdev;
    struct device *dev;
    struct resource *res;
};

static const struct of_device_id of_uart_platform_device_match[] = {
    { .compatible = "bbb,uart2", },
    { }
};

static int cp2102_platform_device_probe(struct platform_device *pdev);
static int cp2102_platform_device_remove(struct platform_device *pdev);
