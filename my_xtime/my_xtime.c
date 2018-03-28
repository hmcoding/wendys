#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Culver");
MODULE_DESCRIPTION("Simple module featuring proc read");

#define ENTRY_NAME "timed"
#define ENTRY_SIZE 100
#define PERMS 0644
#define PARENT NULL
static struct file_operations fops;

static char *message;
static int read_p;
static int prev_seconds;
static int prev_nseconds;

int hello_proc_open(struct inode *sp_inode, struct file *sp_file) {
	printk(KERN_INFO "proc called open\n");
	
	read_p = 1;
	message = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	if (message == NULL) {
		printk(KERN_WARNING "hello_proc_open");
		return -ENOMEM;
	}
	return 0;
}

ssize_t hello_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset) {	
	struct timespec time;
	time = current_kernel_time();
	if (prev_seconds == -1)
	{
		sprintf(message, "current time: %d.%d\n", time.tv_sec, time.tv_nsec);
	}	
	else
	{
		int deltaSeconds;
		int deltaNSeconds;
		// need to borrow
		if (prev_nseconds > time.tv_nsec)
		{
			deltaSeconds = time.tv_sec - prev_seconds - 1;
			deltaNSeconds = time.tv_nsec + 1000000000 - prev_nseconds;
		}
		else
		{
			deltaSeconds = time.tv_sec - prev_seconds;
			deltaNSeconds = time.tv_nsec - prev_nseconds;
		}
		sprintf(message, "current time: %d.%d\nelapsed time: %d.%d\n", 
			time.tv_sec, time.tv_nsec, 
			deltaSeconds, deltaNSeconds);
	}
	// store current time as old time
	prev_seconds = time.tv_sec;
	prev_nseconds = time.tv_nsec;

	int len = strlen(message);
	
	read_p = !read_p;
	if (read_p) {
		return 0;
	}
	
	printk(KERN_INFO "proc called read\n");
	copy_to_user(buf, message, len);
	return len;
}

int hello_proc_release(struct inode *sp_inode, struct file *sp_file) {
	printk(KERN_NOTICE "proc called release\n");
	kfree(message);
	return 0;
}



static int hello_init(void) {
	printk(KERN_NOTICE "/proc/%s create\n", ENTRY_NAME); 
	fops.open = hello_proc_open;
	fops.read = hello_proc_read;
	fops.release = hello_proc_release;
	
	prev_seconds = -1;
	prev_nseconds = -1;

	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops)) {
		printk(KERN_WARNING "proc_create\n");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}
	return 0;
}

//module_init(hello_init); // from his example

static void hello_exit(void) {
	remove_proc_entry(ENTRY_NAME, NULL);
	printk(KERN_NOTICE "Removing /proc/%s.\n", ENTRY_NAME);
}

module_init(hello_init);
module_exit(hello_exit);
