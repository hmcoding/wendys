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

// elevator globals

int current_direction;
int scan_direction;
int should_stop;
int current_floor;
int next_floor;
int current_passengers;
int current_weight;
int waiting_passengers;
int passengers_serviced;
int passengers_serviced_by_floor[NUM_FLOORS];

// execution thread
struct task_struct* elevator_thread;
struct mutex mutex_passenger_queue;
struct mutex mutex_elevator_list;

// utility functions
char* PrintDirection(int dir);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gregulak");
MODULE_DESCRIPTION("Elevator Scheduling");

#define ENTRY_NAME "elevator"
#define PERMS 0644
#define PARENT NULL
static struct file_operations fops;

static char * message;
static int read_p;


int hello_proc_open(struct inode *sp_inode, struct file *sp_file) {
	printk("proc called open\n");
	
	read_p = 1;
	message = kmalloc(sizeof(char) * 2048, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	if (message == NULL) {
		printk("ERROR, hello_proc_open");
		return -ENOMEM;
	}
	
	return 0;
}

ssize_t hello_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset) {	
	int odd;	
	int len;
	current_passengers = ElevatorListSize();
	current_weight = ElevatorWeight(); 

	odd = current_weight % 2;
	if (odd)
	{
	sprintf(message, "The Elevator's Current Movement State Is: %s\nThe Current Floor Is: %d\nThe Next Floor Is: %d\nThe Elevator's current Load Is: %d Passenger Units, %d.5 Weight Units \nNumber of passengers serviced: %d\nPassengersWaiting:\n%s", PrintDirection(current_direction), current_floor, next_floor, current_passengers, current_weight/2, passengers_serviced, PrintQueueToString());
	}
	else
	{
	sprintf(message, "The Elevator's Current Movement State Is: %s\nThe Current Floor Is: %d\nThe Next Floor Is: %d\nThe Elevator's current Load Is: %d Passenger Units, %d Weight Units \nNumber of passengers serviced: %d\nPassengersWaiting:\n%s", PrintDirection(current_direction), current_floor, next_floor, current_passengers, current_weight/2, passengers_serviced, PrintQueueToString());
	}
	read_p = !read_p;
	if (read_p) {
		return 0;
	}
	len = strlen(message);
	printk("proc called read\n");
	copy_to_user(buf, message, len);
	return len;
}

int hello_proc_release(struct inode *sp_inode, struct file *sp_file) {
	printk("proc called release\n");
	kfree(message);	
	return 0;
}

static int hello_init(void) {
	int i;
	printk("Inserting Elevator\n"); 
	printk("/proc/%s create\n", ENTRY_NAME); 
	fops.open = hello_proc_open;
	fops.read = hello_proc_read;
	fops.release = hello_proc_release;

	// initialize elevator globals
	current_direction = STOPPED;
	scan_direction = UP;
	should_stop = 0;
	current_floor = 1;
	next_floor = 1;
	current_passengers = 0;
	current_weight = 0;
	waiting_passengers = 0;
	passengers_serviced = 0;
	for (i = 0; i < NUM_FLOORS; i++)
		passengers_serviced_by_floor[i] = 0;
	InitializeQueue();

	elevator_syscalls_create();
	
	// setup mutex
	mutex_init(&mutex_passenger_queue);
	mutex_init(&mutex_elevator_list);	

	// start the elevator thread
	elevator_thread = kthread_run(ElevatorRun, NULL, "Elevator Thread");
	if (IS_ERR(elevator_thread))
	{
		printk("Error! kthread_run, elevator thread\n");
		return PTR_ERR(elevator_thread);
	}	
	
	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops)) {
		printk("ERROR! proc_create\n");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}
	return 0;
}

static void hello_exit(void) {
	int ret;
	remove_proc_entry(ENTRY_NAME, NULL);
	printk("Removing elevator\n");
	elevator_syscalls_remove();
	CleanupQueue();	
	printk("Removing /proc/%s.\n", ENTRY_NAME);
	ret = kthread_stop(elevator_thread);
	if (ret != -EINTR)
		printk("Elevator thread has stopped.\n");
}

module_init(hello_init);
module_exit(hello_exit);

// utility functions
char* PrintDirection(int dir)
{
	static char buff[32];
	if (dir == IDLE)
		sprintf(buff, "IDLE");
	else if (dir == UP)
		sprintf(buff, "UP");
	else if (dir == DOWN)
		sprintf(buff, "DOWN");
	else if (dir == LOADING)
		sprintf(buff, "LOADING");
	else if (dir == STOPPED)
		sprintf(buff, "STOPPED");
	else
		sprintf(buff, "BAD_ARG_PRINT_DIR");
	return buff;
}
