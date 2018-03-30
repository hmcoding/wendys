#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <syscalls.h>

#define IDLE 0
#define UP 1 
#define DOWN 2
#define LOADING 3
#define OFFLINE 4
#define NUM_FLOORS 10



int nowDirection;
int findDirection;
int shouldOffline;
int nowFloor;
int nextFloor;
int nowPass;
int nowWeight;
int waitPass;
int loadedPass;
int loadedPassFloor[NUM_FLOORS];


struct task_struct* eThread;
struct mutex mutex_pQueue;
struct mutex mutex_eList;


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Culver Perry Davis");
MODULE_DESCRIPTION("Elevator Scheduling");

#define ENTRY_NAME "elevator"
#define PERMS 0644
#define PARENT NULL
static struct file_operations fops;

static char * message;
static int read_p;


int hello_proc_open(struct inode *sp_inode, struct file *sp_file) {
	printk(KERN_INFO "proc called open\n");
	
	read_p = 1;
	message = kmalloc(sizeof(char) * 2048, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	if (message == NULL) {
		printk(KERN_WARNING "hello_proc_open");
		return -ENOMEM;
	}
	
	return 0;
}

ssize_t hello_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset) {	
	
	int len;
	nowPass = sizeElevList();
	nowWeight = getElevWeight(); 

	read_p = !read_p;
	if (read_p) {
		return 0;
	}
	len = strlen(message);
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
	int i;
	printk(KERN_NOTICE "/proc/%s create\n",ENTRY_NAME);
	fops.open = hello_proc_open;
	fops.read = hello_proc_read;
	fops.release = hello_proc_release;

	
	nowDirection = OFFLINE;
	findDirection = UP;
	shouldOffline = 0;
	nowFloor = 1;
	nextFloor = 1;
	nowPass = 0;
	nowWeight = 0;
	waitPass = 0;
	loadedPass = 0;
	
	
	
	i = 0;
	while(i < NUM_FLOORS)
	{
		loadedPassFloor[i] = 0;
		i++;
	}
	
	qInitial();

	elevator_syscalls_create();
	

	mutex_init(&mutex_pQueue);
	mutex_init(&mutex_eList);	

	
	eThread = kthread_run(elevGo, NULL, "Elevator Thread");
	if (IS_ERR(eThread))
	{
		
		return PTR_ERR(eThread);
	}	
	
	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops)) {
		printk(KERN_WARNING "proc create\n");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}
	return 0;
}

static void hello_exit(void) {
	int ret;
	remove_proc_entry(ENTRY_NAME, NULL);
	
	elevator_syscalls_remove();
	qClean();	
	printk(KERN_NOTICE "Removing /proc/%s.\n", ENTRY_NAME);
	ret = kthread_stop(eThread);

}

module_init(hello_init);
module_exit(hello_exit);




