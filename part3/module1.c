#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <syscalls.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gade Hett Duckett");
MODULE_DESCRIPTION("Module for implementing an elevator and scheduler");

/* Define our data structures */

// State of the elevator
typedef enum {
	IDLE,
	LOADING,
	MOVING,
	STOPPED,
} state;

typedef enum {
	UP,
	DOWN,
	NONE
} direction;

// passenger which can be passed from one list (floor) to another (elevator)
struct passenger {
	struct list_head list;
	int type;
	int dest;
};

// floor: holds two lists (up and down) and a mutex as well as state information
struct floor {
	struct mutex floor_mutex;
	struct list_head up;
	struct list_head down;
	int waiting;
	int current_load;
	int serviced;
};

// elevator: holds a list and state information
struct elevator {
	struct list_head compartment;
	int weight;
	int passengers;
	int current_floor;
	int next_floor;
	state status;
	direction dir;
};

// function prototypes
void convert_state_to_string(state status, direction dir, char *buf);
void convert_passenger_to_string(int type, char *buf);
int weight_converter(int passenger_type);
int passenger_converter(int passenger_type);
void display_passengers(struct list_head *list);
void initialize_elevator(void);
void initialize_floors(void);
void elevator_move(void);
void elevator_load(void);
int elevator_run(void *data);
int loader_run(void *data);
int elevator_proc_open(struct inode *sp_inode, struct file *sp_file);
ssize_t elevator_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset);
int elevator_proc_release(struct inode *sp_inode, struct file *sp_file);
void elevator_syscalls_create(void);
void elevator_syscalls_remove(void);
static int elevator_init(void);
static void elevator_exit(void);
void change_state(void);
void unload_passengers(void);
void load_passengers(void);
void change_state(void);
void scan_for_next_floor_if_not_empty(void);
int scan_for_next_floor_if_empty(void);
void remove_passenger_from_elevator(struct passenger *entry);
void insert_passenger_into_elevator(struct passenger *entry, int current_floor);
int exceeds_capacity(struct passenger *entry);
void display_weight(int weight, char *buf);

// procfs information
#define ENTRY_NAME "elevator"
#define PERMS 0644
#define PARENT NULL

static struct file_operations fops;

// default values and thread names
#define BUFFER_SIZE 1024
#define NUMBER_OF_FLOORS 10
#define MAX_WEIGHT 15
#define MAX_PASSENGERS 10
#define KTHREAD_1 "kthread elevator"
#define KTHREAD_2 "kthread loader"
#define KFLAGS (__GFP_RECLAIM | __GFP_IO | __GFP_FS)

// declaration of global variables
static struct task_struct *kt_1;
static struct task_struct *kt_2;

static char *message;
static int read_p;
static int starting;
static int running;
static int stopping;

struct floor floors[NUMBER_OF_FLOORS];
struct elevator *elev;
struct mutex elev_mutex;
struct mutex shared_data_mutex;

// convert the the direction to a string for display
void convert_state_to_string(state status, direction dir, char *buf) {
	switch (status) {
		case IDLE:
			strcpy(buf, "IDLE");
			break;
		case MOVING:
			if (dir == (direction) UP) {
				strcpy(buf, "UP");
			} else if (dir == (direction) DOWN) {
				strcpy(buf, "DOWN");
			} else {
				strcpy(buf, "BAD DIR");
			}
			break;
		case LOADING:
			strcpy(buf, "LOADING");
			break;
		case STOPPED:
			strcpy(buf, "STOPPED");
			break;
		default:
			strcpy(buf, "BAD");
			break;
	}
}

// adjust the weight to display the actual weight for the problem
void display_weight(int weight, char *buf) {
	char b[10];
	strcpy(buf, "");
	sprintf(b, "%d", weight/2);
	strcat(buf, b);
	if (weight%2) {
		sprintf(b, ".5");
		strcat(buf, b);
	}
	strcat(buf, "\n");
}

// convert the passenger type to a string
void convert_passenger_to_string(int type, char *buf) {
	switch (type) {
		case 1:
			strcpy(buf, "adult");
			break;
		case 2:
			strcpy(buf, "child");
			break;
		case 4:
			strcpy(buf, "bellhop");
			break;
		case 3:
			strcpy(buf, "room service\0");
			break;
		default:
			strcpy(buf, "");
	}
}

// convert passenger type to weight
int weight_converter(int passenger_type) {
	switch (passenger_type) {
		case 1:
			return 1;
		case 2:
			return 0.5; 
		case 3:
			return 2;
		case 4:
			return 3;
		default:
			return 0;
	}
}

// convert passenger type to passenger size
int passenger_converter(int passenger_type) {
	switch (passenger_type) {
		case 1:
			return 1;
		case 2:
			return 1;
		case 3:
			return 2;
		case 4:
			return 2;
		default:
			return 0;
	}
}

// display the number of passenger and put it on the global message variable
void display_passengers(struct list_head *list) {
	char buf[40];
	int first_p = 1;
	struct list_head *ptr;
	struct passenger *entry;

	list_for_each(ptr, list) {
		entry = list_entry(ptr, struct passenger, list);
		if (first_p) {
			first_p = 0;
			convert_passenger_to_string(entry->type, buf);
			strcat(message, buf);
			sprintf(buf, " to %u", entry->dest + 1);
			strcat(message, buf);
		} else {
			strcat(message, ", ");
			convert_passenger_to_string(entry->type, buf);
			strcat(message, buf);
			sprintf(buf, " to %u", entry->dest + 1);
			strcat(message, buf);
		}
	}
}

// set the elevator when the start_elevator() syscall is called
void initialize_elevator() {
	mutex_lock_interruptible(&shared_data_mutex);
	running = 1;
	mutex_unlock(&shared_data_mutex);

	mutex_lock_interruptible(&elev_mutex);
	elev->current_floor = 1;
	elev->next_floor = -1;
	elev->passengers = 0;
	elev->weight = 0;
	elev->status = IDLE;
	elev->dir = NONE;
	list_del_init(&elev->compartment);
	mutex_unlock(&elev_mutex);
}

// set the floors when the module is loaded
void initialize_floors(void) {
	int i;
	for (i = 0; i < NUMBER_OF_FLOORS; ++i) {
		mutex_lock_interruptible(&floors[i].floor_mutex);
		floors[i].waiting = 0;
		floors[i].current_load = 0;
		floors[i].serviced = 0;
		mutex_unlock(&floors[i].floor_mutex);
	}
}

// look for the next floor if the elevator is not empty
void scan_for_next_floor_if_not_empty(void) {
	int closest_floor;
	struct list_head *ptr;
	struct passenger *entry;

	mutex_lock_interruptible(&elev_mutex);
	closest_floor = elev->current_floor;
	if (elev->dir == UP || elev->dir == NONE) {
		closest_floor = NUMBER_OF_FLOORS - 1;
	} else if (elev->dir == DOWN) {
		closest_floor = 0;
	}
	list_for_each(ptr, &elev->compartment) {
		entry = list_entry(ptr, struct passenger, list);
		if ((elev->dir == UP || elev->dir == NONE) && entry->dest < closest_floor) {
			closest_floor = entry->dest;
		} else if (elev->dir == DOWN && entry->dest > closest_floor) {
			closest_floor = entry->dest;
		}
	}
	elev->next_floor = closest_floor;
	mutex_unlock(&elev_mutex);
}

// look for the next floor if the elevator is empty
int scan_for_next_floor_if_empty(void) {
	int i, up_floor, down_floor, current_floor;
	int up_floor_winner, down_floor_winner;
	int ret;
	direction current_dir;

	ret = 0;
	mutex_lock_interruptible(&elev_mutex);
	current_floor = elev->current_floor;
	current_dir = elev->dir;
	mutex_unlock(&elev_mutex);
	up_floor_winner = -1;
	down_floor_winner = -1;
	for (i = 0; i < NUMBER_OF_FLOORS; ++i) {
		up_floor = current_floor + i;
		down_floor = current_floor - i;
		if (up_floor < NUMBER_OF_FLOORS) {
			mutex_lock_interruptible(&floors[up_floor].floor_mutex);
			if (!list_empty(&floors[up_floor].up)) {
				up_floor_winner = up_floor;
			} else if (!list_empty(&floors[up_floor].down)) {
				up_floor_winner = up_floor;
			}
			mutex_unlock(&floors[up_floor].floor_mutex);
		}
		if (down_floor >= 0) {
			mutex_lock_interruptible(&floors[down_floor].floor_mutex);
			if (!list_empty(&floors[down_floor].down)) {
				down_floor_winner = down_floor;
			} else if (!list_empty(&floors[down_floor].up)) {
				down_floor_winner = down_floor;
			}
			mutex_unlock(&floors[down_floor].floor_mutex);
		}
	}
	mutex_lock_interruptible(&elev_mutex);
	mutex_lock_interruptible(&floors[0].floor_mutex);
	if (elev->current_floor == 0 && !list_empty(&floors[0].up)) {
		up_floor_winner = 0;
	}
	mutex_unlock(&floors[0].floor_mutex);
	mutex_lock_interruptible(&floors[NUMBER_OF_FLOORS - 1].floor_mutex);
	if(elev->current_floor == NUMBER_OF_FLOORS - 1 && !list_empty(&floors[NUMBER_OF_FLOORS - 1].down)) {
		down_floor_winner = NUMBER_OF_FLOORS - 1;
	}
	mutex_unlock(&floors[NUMBER_OF_FLOORS - 1].floor_mutex);
	mutex_unlock(&elev_mutex);
	if (current_dir == UP || current_dir == NONE) {
		if (up_floor_winner != -1) {
			mutex_lock_interruptible(&elev_mutex);
			elev->next_floor = up_floor_winner;
			ret = 1;
			mutex_unlock(&elev_mutex);
		} else if (down_floor_winner != -1) {
			mutex_lock_interruptible(&elev_mutex);
			elev->next_floor = down_floor_winner;
			ret = 1;
			mutex_unlock(&elev_mutex);
		}
	} else if (current_dir == DOWN) {
		if (down_floor_winner != -1) {
			mutex_lock_interruptible(&elev_mutex);
			elev->next_floor = down_floor_winner;
			ret = 1;
			mutex_unlock(&elev_mutex);
		} else if (up_floor_winner != -1 ) {
			mutex_lock_interruptible(&elev_mutex);
			elev->next_floor = up_floor_winner;
			ret = 1;
			mutex_unlock(&elev_mutex);
		}
	}
	return ret;
}

// push the elevator in the correct direction and sleep for 2 seconds
void elevator_move(void) {
	mutex_lock_interruptible(&elev_mutex);
	if (elev->dir == UP) {
		++elev->current_floor;
		if (elev->current_floor == NUMBER_OF_FLOORS - 1) {
			elev->dir = DOWN;
		}
	} else if (elev->dir == DOWN) {
		--elev->current_floor;
		if (elev->current_floor == 0) {
			elev->dir = UP;
		}
	}
	mutex_unlock(&elev_mutex);
	ssleep(2);
}

// sleep for 1 second when the elevator is loading
void elevator_load() {
	ssleep(1);
}

// function which controls the next state of the elevator
void change_state(void) {
	int current_floor, dir, found;
	struct passenger *entry;
	state status;

	mutex_lock_interruptible(&elev_mutex);
	status = elev->status;
	if (status == IDLE) {
		mutex_unlock(&elev_mutex);
		found = scan_for_next_floor_if_empty();
		mutex_lock_interruptible(&elev_mutex);
		if ((elev->next_floor == -1) || !found) {
			mutex_unlock(&elev_mutex);
			schedule();
			mutex_lock_interruptible(&elev_mutex);
		} else {
			if (elev->current_floor < elev->next_floor) {
				elev->status = MOVING;
				elev->dir = UP;
			} else if (elev->current_floor > elev->next_floor) {
				elev->status = MOVING;
				elev->dir = DOWN;
			} else {
				elev->status = LOADING;
			}
		}
	} else if (status == MOVING) {
		mutex_unlock(&elev_mutex);
		elevator_move();
		mutex_lock_interruptible(&elev_mutex);
		current_floor = elev->current_floor;
		dir = elev->dir;
		mutex_lock_interruptible(&floors[current_floor].floor_mutex);
		if (elev->current_floor == elev->next_floor) {
			elev->status = LOADING;
		} else if (dir == UP && !list_empty(&floors[current_floor].up)) {
			entry = list_first_entry(&floors[current_floor].up, struct passenger, list);
			if (!exceeds_capacity(entry)) {
				elev->status = LOADING;
			}
		} else if (dir == DOWN && !list_empty(&floors[current_floor].down)) {
			entry = list_first_entry(&floors[current_floor].down, struct passenger, list);
			if (!exceeds_capacity(entry)) {
				elev->status = LOADING;
			}
		}
		mutex_unlock(&floors[current_floor].floor_mutex);
	} else if (status == LOADING) {
		mutex_unlock(&elev_mutex);
		elevator_load();
		mutex_lock_interruptible(&elev_mutex);
		if (list_empty(&elev->compartment)) {
			elev->status = IDLE;
			mutex_unlock(&elev_mutex);
			schedule();
			mutex_lock_interruptible(&elev_mutex);
		} else {
			mutex_unlock(&elev_mutex);
			scan_for_next_floor_if_not_empty();
			mutex_lock_interruptible(&elev_mutex);
			if (elev->current_floor < elev->next_floor) {
				elev->status = MOVING;
				elev->dir = UP;
			} else if (elev->current_floor > elev->next_floor) {
				elev->status = MOVING;
				elev->dir = DOWN;
			} else {
				elev->status = LOADING;
			}
		}
	} else if (status == STOPPED) {
	}
	mutex_unlock(&elev_mutex);
}

// function which the elevator thread runs
int elevator_run(void *data) {
	while (!kthread_should_stop()) {
		schedule();
		mutex_lock_interruptible(&shared_data_mutex);
		if (stopping) {
			running = 0;
			mutex_unlock(&shared_data_mutex);
			change_state();
			mutex_lock_interruptible(&shared_data_mutex);
			mutex_lock_interruptible(&elev_mutex);
			if (list_empty(&elev->compartment)) {
				stopping = 0;
				elev->status = STOPPED;
			}
			mutex_unlock(&elev_mutex);
			mutex_unlock(&shared_data_mutex);
			mutex_lock_interruptible(&shared_data_mutex);
		} else if (running) {
			if (starting) {
				mutex_unlock(&shared_data_mutex);
				initialize_elevator();
				mutex_lock_interruptible(&shared_data_mutex);
				starting = 0;
			} else {
				mutex_unlock(&shared_data_mutex);
				change_state();
				mutex_lock_interruptible(&shared_data_mutex);
			}
		}
		mutex_unlock(&shared_data_mutex);
	}
	return 1;
}

// removes and frees a passenger
void remove_passenger_from_elevator(struct passenger *entry) {
	mutex_lock_interruptible(&elev_mutex);
	list_del(&entry->list);
	elev->weight -= weight_converter(entry->type);
	elev->passengers -= passenger_converter(entry->type);
	kfree(entry);
	mutex_unlock(&elev_mutex);
}

// inserts a passenger into the elevator
void insert_passenger_into_elevator(struct passenger *entry, int current_floor) {
	mutex_lock_interruptible(&floors[current_floor].floor_mutex);
	mutex_lock_interruptible(&elev_mutex);
	list_move_tail(&entry->list, &elev->compartment);
	printk("successful transfer\n");
	elev->weight += weight_converter(entry->type);
	elev->passengers += passenger_converter(entry->type);
	floors[current_floor].current_load -= weight_converter(entry->type);
	floors[current_floor].waiting -= 1;
	floors[current_floor].serviced += 1;
	mutex_unlock(&elev_mutex);
	mutex_unlock(&floors[current_floor].floor_mutex);
}

// looks at each passenger on the elevator and unloads whoever needs to be dropped off
void unload_passengers(void) {
	int current_floor;
	struct list_head *ptr, *tmp;
	struct passenger *entry;

	mutex_lock_interruptible(&elev_mutex);
	current_floor = elev->current_floor;
	list_for_each_safe(ptr, tmp, &elev->compartment) {
		entry = list_entry(ptr, struct passenger, list);
		if (entry->dest == current_floor) {
			mutex_unlock(&elev_mutex);
			remove_passenger_from_elevator(entry);
			mutex_lock_interruptible(&elev_mutex);
		}
	}
	mutex_unlock(&elev_mutex);
}

// checks if the next passenger will break the elevator
int exceeds_capacity(struct passenger *entry) {
	int ret, pass_weight, pass_size;
	pass_weight = weight_converter(entry->type);
	pass_size = passenger_converter(entry->type);
	if ((((pass_weight + elev->weight) > MAX_WEIGHT)) || (((pass_size + elev->passengers) > MAX_PASSENGERS))) {
		ret = 1;
	} else {
		ret = 0;
	}
	return ret;
}

// inserting passengers onto the elevator
void load_passengers(void) {
	int current_floor, exceed_limit;
	struct passenger *entry;

	mutex_lock_interruptible(&elev_mutex);
	current_floor = elev->current_floor;
	exceed_limit = 0;
	mutex_lock_interruptible(&floors[current_floor].floor_mutex);
	if (elev->dir == UP || elev->dir == NONE) {
		while (!exceed_limit && !list_empty(&floors[current_floor].up)) {
			entry = list_first_entry(&floors[current_floor].up, struct passenger, list);
			exceed_limit = exceeds_capacity(entry);
			if (!exceed_limit) {
				printk("about to transfer list\n");
				list_move_tail(&entry->list, &elev->compartment);
				printk("successful transfer\n");
				elev->weight += weight_converter(entry->type);
				elev->passengers += passenger_converter(entry->type);
				floors[current_floor].current_load -= weight_converter(entry->type);
				floors[current_floor].waiting -= 1;
				floors[current_floor].serviced += 1;
			}
		}
		if (list_empty(&elev->compartment)) {
			while (!exceed_limit && !list_empty(&floors[current_floor].down)) {
				entry = list_first_entry(&floors[current_floor].down, struct passenger, list);
				exceed_limit = exceeds_capacity(entry);
				if (!exceed_limit) {
					printk("about to transfer list\n");
					list_move_tail(&entry->list, &elev->compartment);
					printk("successful transfer\n");
					elev->weight += weight_converter(entry->type);
					elev->passengers += passenger_converter(entry->type);
					elev->dir = DOWN;
					floors[current_floor].current_load -= weight_converter(entry->type);
					floors[current_floor].waiting -= 1;
					floors[current_floor].serviced += 1;
				}
			}
		}
	} else if (elev->dir == DOWN) {
		while (!exceed_limit && !list_empty(&floors[current_floor].down)) {
			entry = list_first_entry(&floors[current_floor].down, struct passenger, list);
			exceed_limit = exceeds_capacity(entry);
			if (!exceed_limit) {
				list_move_tail(&entry->list, &elev->compartment);
				printk("successful transfer\n");
				elev->weight += weight_converter(entry->type);
				elev->passengers += passenger_converter(entry->type);
				floors[current_floor].current_load -= weight_converter(entry->type);
				floors[current_floor].waiting -= 1;
				floors[current_floor].serviced += 1;
			}
		}
		if (list_empty(&elev->compartment)) {
			while (!exceed_limit && !list_empty(&floors[current_floor].up)) {
				entry = list_first_entry(&floors[current_floor].up, struct passenger, list);
				exceed_limit = exceeds_capacity(entry);
				if (!exceed_limit) {
					list_move_tail(&entry->list, &elev->compartment);
					printk("successful transfer\n");
					elev->weight += weight_converter(entry->type);
					elev->passengers += passenger_converter(entry->type);
					elev->dir = UP;
					floors[current_floor].current_load -= weight_converter(entry->type);
					floors[current_floor].waiting -= 1;
					floors[current_floor].serviced += 1;
				}
			}
		}
	}
	mutex_unlock(&floors[current_floor].floor_mutex);
	mutex_unlock(&elev_mutex);
}

// loader thread's implementation
int loader_run(void *data) {
	while (!kthread_should_stop()) {
		schedule();
		mutex_lock_interruptible(&elev_mutex);
		if (elev->status == LOADING) {
			if (elev->current_floor == elev->next_floor && !list_empty(&elev->compartment)) {
				mutex_unlock(&elev_mutex);
				printk("trying to unload\n");
				unload_passengers();
				mutex_lock_interruptible(&elev_mutex);
			}
			mutex_unlock(&elev_mutex);
			mutex_lock_interruptible(&shared_data_mutex);
			if (running) {
				mutex_unlock(&shared_data_mutex);
				load_passengers();
				mutex_lock_interruptible(&shared_data_mutex);
			}
			mutex_unlock(&shared_data_mutex);
			mutex_lock_interruptible(&elev_mutex);
		} else if (elev->status == STOPPED) {
		}
		mutex_unlock(&elev_mutex);
	}
	return 0;
}

// open function for proc
int elevator_proc_open(struct inode *sp_inode, struct file *sp_file) {
	int i, first_p;
	char buf[40];
	struct list_head *ptr;
	struct passenger *entry;
	
	read_p = 1;
	message = kmalloc(sizeof(char) * BUFFER_SIZE, KFLAGS);
	if (message == NULL) {
		printk("ERROR, counter_proc_open");
		return -ENOMEM;
	}
	strcpy(message, "");
	mutex_lock_interruptible(&elev_mutex);
	strcat(message, "*Elevator Info*\nMovement state: ");
	convert_state_to_string(elev->status, elev->dir, buf);
	strcat(message, buf);
	strcat(message, "\ncurrent floor: ");
	sprintf(buf, "%u\n", elev->current_floor + 1);
	strcat(message, buf);
	strcat(message, "next floor: ");
	sprintf(buf, "%u\n", elev->next_floor + 1);
	strcat(message, buf);
	strcat(message, "current load: passengers: ");
	sprintf(buf, "%u ", elev->passengers);
	strcat(message, buf);
	strcat(message, "weight: ");
	display_weight(elev->weight, buf);
	strcat(message, buf);
	strcat(message, "on elevator: ");
	first_p = 1;
	list_for_each(ptr, &elev->compartment) {
		entry = list_entry(ptr, struct passenger, list);
		if (first_p) {
			first_p = 0;
			sprintf(buf, "[P:%d,D:%d]", entry->type, entry->dest + 1);
			strcat(message, buf);
		}
		else {
			sprintf(buf, ", [P:%d,D:%d]", entry->type, entry->dest + 1);
			strcat(message, buf);
		}
	}
	strcat(message, "\n");
	mutex_unlock(&elev_mutex);

	for (i = 0; i < NUMBER_OF_FLOORS; ++i) {
		mutex_lock_interruptible(&floors[i].floor_mutex);
		sprintf(buf, "Passengers for floor %u:\n", i + 1);
		strcat(message, buf);
		sprintf(buf, "serviced: %u\n", floors[i].serviced);
		strcat(message, buf);
		strcat(message, "going up: ");
		display_passengers(&floors[i].up);
		strcat(message, "\ngoing down: ");
		display_passengers(&floors[i].down);
		strcat(message, "\n");
		mutex_unlock(&(floors[i].floor_mutex));
	}
	return 0;
}

// read function for proc
ssize_t elevator_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset){
	int len = strlen(message);
	read_p = !read_p;
	if (read_p) {
		return 0;
	}
	copy_to_user(buf, message, len);
	return len;
}

// close function for proc
int elevator_proc_release(struct inode *sp_inode, struct file *sp_file) {
	kfree(message);
	return 0;
}

// syscall implemenation for putting the elevator in the running mode
extern long (*STUB_start_elevator)(void);
long start_elevator(void) {
	mutex_lock_interruptible(&shared_data_mutex);
	if (running) {
		mutex_unlock(&shared_data_mutex);
		return 1;
	} else {
		running = 1;
		starting = 1;
	}
	mutex_unlock(&shared_data_mutex);
	printk("Starting elevator\n");
	return 0;
}

// syscall implemenation for inserting a passenger on a floor
extern long (*STUB_issue_request)(int,int,int);
long issue_request(int passenger_type, int start_floor, int destination_floor) {
	int going_up, actual_start_floor, actual_destination_floor;
	struct passenger *new;
	printk("New request: %d, %d => %d\n", passenger_type, start_floor, destination_floor);
	if ((passenger_type < 0) || (passenger_type > 3) || (start_floor < 1) || (start_floor > NUMBER_OF_FLOORS) || (destination_floor < 1) || (destination_floor > NUMBER_OF_FLOORS)) {
		printk("bad request\n");
		return 1;
	}
	actual_start_floor = start_floor - 1;
	actual_destination_floor = destination_floor - 1;
	new = kmalloc(sizeof(struct passenger), KFLAGS);
	new->type = passenger_type;
	new->dest = actual_destination_floor;
	mutex_lock_interruptible(&elev_mutex);
	if (elev->dir == UP || elev->dir == NONE) {
		if (start_floor <= destination_floor) {
			going_up = 1;
		} else {
			going_up = 0;
		}
	} else if (elev->dir == DOWN) {
		if (start_floor < destination_floor) {
			going_up = 1;
		} else {
			going_up = 0;
		}
	} else {
		if (start_floor <= destination_floor) {
			going_up = 1;
		} else {
			going_up = 0;
		}
	}
	mutex_unlock(&elev_mutex);

	mutex_lock_interruptible(&floors[actual_start_floor].floor_mutex);
	if (going_up) {
		list_add_tail(&(new->list), &floors[actual_start_floor].up);
	} else {
		list_add_tail(&(new->list), &floors[actual_start_floor].down);
	}
	floors[actual_start_floor].waiting += 1;
	floors[actual_start_floor].current_load += weight_converter(new->type);
	mutex_unlock(&floors[actual_start_floor].floor_mutex);

	return 0;
}

// syscall implementation for setting the mode to stopping
extern long (*STUB_stop_elevator)(void);
long stop_elevator(void) {
	mutex_lock_interruptible(&shared_data_mutex);
	if (stopping) {
		mutex_unlock(&shared_data_mutex);
		return 1;
	} else if (running) {
		stopping = 1;
	}
	mutex_unlock(&shared_data_mutex);
	printk("Stopping elevator\n");
	return 0;
}

// sets the syscalls to another function
void elevator_syscalls_create(void) {
	STUB_start_elevator =& (start_elevator);
	STUB_issue_request =& (issue_request);
	STUB_stop_elevator =& (stop_elevator);
}

// sets the syscalls to NULL
void elevator_syscalls_remove(void) {
	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_stop_elevator = NULL;
}

// Function which is run when the module is loaded
static int elevator_init(void) {
	int i;
	printk("Inserting Elevator\n");
	elevator_syscalls_create();
	for (i = 0; i < NUMBER_OF_FLOORS; ++i) {
		mutex_init(&floors[i].floor_mutex);
		INIT_LIST_HEAD(&floors[i].up);
		INIT_LIST_HEAD(&floors[i].down);
	}
	mutex_init(&elev_mutex);
	mutex_init(&shared_data_mutex);
	elev = kmalloc(sizeof(struct elevator), KFLAGS);
	INIT_LIST_HEAD(&elev->compartment);
	elev->weight = 0;
	elev->passengers = 0;
	elev->current_floor = 0;
	elev->next_floor = -1;
	elev->status = STOPPED;
	elev->dir = NONE;
	running = 0;
	stopping = 0;
	initialize_floors();

	kt_1 = kthread_run(elevator_run, (void *) KTHREAD_1, KTHREAD_1);
	if (IS_ERR(kt_1)) {
		printk("ERROR! kthread_run, %s\n", KTHREAD_1);
		return PTR_ERR(kt_1);
	}
	kt_2 = kthread_run(loader_run, (void *) KTHREAD_2, KTHREAD_2);
	if (IS_ERR(kt_2)) {
		printk("ERROR! kthread_run, %s\n", KTHREAD_2);
		return PTR_ERR(kt_2);
	}

	printk("/proc/%s create\n", ENTRY_NAME);
	fops.open = elevator_proc_open;
	fops.read = elevator_proc_read;
	fops.release = elevator_proc_release;
	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops)) {
		printk("ERROR! proc_create\n");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}
	return 0;
}

// function which run when the module is removed
static void elevator_exit(void) {
	int i, ret;
	struct list_head *tmp, *ptr;
	struct passenger *entry;

	printk("Removing elevator\n");
	elevator_syscalls_remove();

	ret = kthread_stop(kt_1);
	if (ret != -EINTR) {
		printk("%s has stopped\n", KTHREAD_1);
	}
	ret = kthread_stop(kt_2);
	if (ret != -EINTR) {
		printk("%s has stopped\n", KTHREAD_2);
	}

	for (i = 0; i < NUMBER_OF_FLOORS; ++i) {
		list_for_each_safe(ptr, tmp, &floors[i].up) {
			entry = list_entry(ptr, struct passenger, list);
			list_del(ptr);
			kfree(entry);
		}
		list_for_each_safe(ptr, tmp, &floors[i].down) {
			entry = list_entry(ptr, struct passenger, list);
			list_del(ptr);
			kfree(entry);
		}
	}
	list_for_each_safe(ptr, tmp, &elev->compartment) {
		entry = list_entry(ptr, struct passenger, list);
		list_del(ptr);
		kfree(entry);
	}
	kfree(elev);
	remove_proc_entry(ENTRY_NAME, NULL);
	printk("Removing /proc/%s.\n", ENTRY_NAME);
}


module_init(elevator_init);
module_exit(elevator_exit);
