// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <generated/utsrelease.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timekeeping.h>


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Hello version module");
MODULE_AUTHOR("Ngoc Dai");

static char *who = "Ngoc Dai";
module_param(who, charp, 0644);
MODULE_PARM_DESC(who, "Recipient of the hello message");

static int howmany = 10;
module_param(howmany, int, 0644);
MODULE_PARM_DESC(howmany, "Number of greeting");

time64_t time;

static int __init hello_init(void)
{
    time = ktime_get_seconds();
    pr_alert("Hello %s. You are currently using Linux %s.\n", who,  UTS_RELEASE);

    for (int i = 0; i < howmany; i++) {
        printk("Hello.\n");
    }

    return 0;
}

static void __exit hello_exit(void)
{
    time64_t duration = ktime_get_seconds() - time;

    pr_alert("Goodbye %s!. We have %lld second(s) spending together\n", who, duration);
}

module_init(hello_init);
module_exit(hello_exit);
