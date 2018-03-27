#include <syscalls.h>
#include <linux/printk.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/init.h>
#include <linux/module.h>

#define KFLAGS (__GFP_RECLAIM | __GFP_IO | __GFP_FS) 

#define IDLE 0
#define UP 1
#define DOWN 2
#define LOADING 3
#define STOPPED 4

#define NUM_FLOORS 10

// struct definitions
struct queue_entry {
	struct list_head list;
	int m_type;
	int m_startingFloor;
	int m_destinationFloor;
};

static struct list_head passenger_queue[NUM_FLOORS];
static struct list_head elevator_list;

// utility functions
int PassengerWeight(int type);

//added code here
//Initializing all of the information that has to be printed
extern int current_direction;  
extern int scan_direction;
extern int should_stop;
extern int current_floor;
extern int next_floor;
extern int current_passengers;
extern int current_weight;
extern int waiting_passengers;
extern int passengers_serviced;
extern int passengers_serviced_by_floor[NUM_FLOORS];

//above

// global thread and mutex
extern struct mutex mutex_passenger_queue;
extern struct mutex mutex_elevator_list;
extern struct task_struct* elevator_thread;

extern long (*STUB_start_elevator)(void);
long start_elevator(void) {
	if (should_stop)
	{
		should_stop = 0;
		return 0;
	}
	else if (current_direction == STOPPED)
	{
		printk("Starting elevator\n");
		current_direction = IDLE;
		return 0;
	}
	else
	{
		return 1;
	}
}

extern long (*STUB_issue_request)(int,int,int);
long issue_request(int passenger_type, int start_floor, int destination_floor) {
	printk("New request: %d, %d => %d\n", passenger_type, start_floor, destination_floor);
	if (start_floor == destination_floor)
	{
		passengers_serviced++;
		passengers_serviced_by_floor[start_floor - 1]++;	
	}
	else
	{
		QueuePassenger(passenger_type, start_floor, destination_floor);
	}
	return 0;
}

extern long (*STUB_stop_elevator)(void);
long stop_elevator(void) {
	printk("Stopping elevator\n");
	if (should_stop == 1)
		return 1;
	should_stop = 1;
	return 0;
}





void elevator_syscalls_create(void) {
	// changed
	STUB_issue_request = &(issue_request);
	// changed
	STUB_start_elevator = &(start_elevator);
	STUB_stop_elevator = &(stop_elevator);
}

void elevator_syscalls_remove(void) {
	// changed
	STUB_issue_request = NULL;
	// changed
	STUB_start_elevator = NULL;
	STUB_stop_elevator = NULL;
}

// utility functions
void InitializeQueue(void)
{
	int i;
	for(i = 0; i < NUM_FLOORS; i++)
		INIT_LIST_HEAD(&passenger_queue[i]);
	INIT_LIST_HEAD(&elevator_list);
}

void CleanupQueue(void)
{
	struct queue_entry *ent;
	struct list_head *pos, *q;
	int i;

	mutex_lock_interruptible(&mutex_passenger_queue);
	for (i = 0; i < NUM_FLOORS; i++)
	{
		list_for_each_safe(pos, q, &passenger_queue[i])
		{
			ent = list_entry(pos, struct queue_entry, list);
			list_del(pos);
			kfree(ent);
		}
	}
	mutex_unlock(&mutex_passenger_queue);
	mutex_lock_interruptible(&mutex_elevator_list);
	list_for_each_safe(pos, q, &elevator_list)
	{
		ent = list_entry(pos, struct queue_entry, list);
		list_del(pos);
		kfree(ent);
	}
	mutex_unlock(&mutex_elevator_list);
}

void QueuePassenger(int type, int start, int dest)
{
	struct queue_entry *new_entry;
	new_entry = kmalloc(sizeof(struct queue_entry), KFLAGS);
	new_entry->m_type = type;
	new_entry->m_startingFloor = start;
	new_entry->m_destinationFloor = dest;
	mutex_lock_interruptible(&mutex_passenger_queue);
	list_add_tail(&new_entry->list, &passenger_queue[start - 1]);
	mutex_unlock(&mutex_passenger_queue);
	PrintQueue();
//	PrintElevator();
//	current_direction = UP;
//	LoadAPassenger();
//	PrintElevator();
//	PrintQueue();
//	current_direction = STOPPED;
}

void PrintQueue(void)
{
	struct list_head *pos;
	struct queue_entry* entry;
	int currentPos;
	int i;
	currentPos = 0;
	printk("Passenger Queue:\n");
	mutex_lock_interruptible(&mutex_passenger_queue);
	for (i = 0; i < NUM_FLOORS; i++)
	{
		printk("Floor: %d\n", i+1);
		list_for_each(pos, &passenger_queue[i])
		{
			entry = list_entry(pos, struct queue_entry, list);
			printk("Queue pos: %d\nType: %d\nStart Floor: %d\nDest Floor: %d\n",
				currentPos, entry->m_type, entry->m_startingFloor,
				entry->m_destinationFloor);
			++currentPos;
		}
	}
	mutex_unlock(&mutex_passenger_queue);
	printk("\n");
}

char* PrintQueueToString(void)
{
	int currentPos;
	static char buffer[1024];
	static char tempbuf[256];
	int i;
	int numPassengers;
	int weight;
	int odd;
	currentPos = 0;
	sprintf(buffer, "Passenger Queue:\n");
	for (i = 1; i <= NUM_FLOORS; i++)
	{
		sprintf(tempbuf, "Floor: %d\n", i);
		strcat(buffer, tempbuf);
		numPassengers = PassengerQueueSize(i);
		weight = PassengerQueueWeight(i);
		odd = weight % 2;
		if (odd)
			sprintf(tempbuf, "Current Load: %d Passenger Units, %d.5 Weight Units\nNumber of Passengers Serviced: %d\n",
				numPassengers, weight/2, passengers_serviced_by_floor[i-1]);
		else
			sprintf(tempbuf, "Current Load: %d Passenger Units, %d Weight Units\nNumber of Passengers Serviced: %d\n",
				numPassengers, weight/2, passengers_serviced_by_floor[i-1]);

		strcat(buffer, tempbuf);
	}
	strcat(buffer, "\n");
	return buffer;
}

void PrintElevator(void)
{
	struct list_head *pos;
	struct queue_entry* entry;
	int currentPos;
	currentPos = 0;
	printk("Elevator List:\n");
	mutex_lock_interruptible(&mutex_elevator_list);
	list_for_each(pos, &elevator_list)
	{
		entry = list_entry(pos, struct queue_entry, list);
		printk("Elevator pos: %d\nType: %d\nStart Floor: %d\nDest Floor: %d\n",
			currentPos, entry->m_type, entry->m_startingFloor,
			entry->m_destinationFloor);
		++currentPos;
	}
	mutex_unlock(&mutex_elevator_list);
	printk("\n");
}

int PassengerWeight(int type)
{
	if (type == 0)
		return 2;
	else if (type == 1)
		return 1;
	else if (type == 2)
		return 4;
	else if (type == 3)
		return 4;
	else
		return 0;
}

int ElevatorRun(void* data)
{
	while (!kthread_should_stop())
	{
		// state 1, STOPPED, do nothing
		if (current_direction == STOPPED)
		{
			// do nothing
		}	
		
		// state 2, IDLE, waiting for passengers
		// assuming on floor 1
		else if (current_direction == IDLE)
		{
			scan_direction = UP;
			if (ShouldLoad() && !should_stop)
				current_direction = LOADING;
			else
			{
				current_direction = UP;
				next_floor = current_floor + 1;
			}
		}
	
		// state 3, UP, moving upwards
		else if (current_direction == UP)
		{
			MoveElevator(next_floor);
			if (current_floor == 10)
			{
				scan_direction = DOWN;
				current_direction = DOWN;
			}
			if ((ShouldLoad() && !should_stop) || ShouldUnload())
				current_direction = LOADING;
			else if (current_floor == 10)
			{
				next_floor = current_floor - 1;
			}
			else
			{
				next_floor = current_floor + 1;
			}
		}
	
		// state 4, DOWN, moving downwards
		else if (current_direction == DOWN)
		{
			MoveElevator(next_floor);
			if (current_floor == 1)
			{
				scan_direction = UP;
				current_direction = UP;
			}
			// stop logic and re-initialize logic
			if (should_stop && !ElevatorListSize() && current_floor == 1)
			{
				current_direction = STOPPED;
				should_stop = 0;
				scan_direction = UP;
			}
			else if ((ShouldLoad() &&!should_stop) || ShouldUnload())
				current_direction = LOADING;
			else if (current_floor == 1)
			{
				next_floor = current_floor + 1;
			}
			else
			{
				next_floor = current_floor - 1;
			}	
		}
	
		// state 5, LOADING, loading passengers
		else if (current_direction == LOADING)
		{
			ssleep(1);
			UnloadPassengers();
			while (ShouldLoad() && !should_stop)
				FIFOLoadSinglePassenger(current_floor);
			current_direction = scan_direction;
			if (current_direction == DOWN)
			{
				if (current_floor == 1)
				{
					scan_direction = UP;
					current_direction = UP;
					next_floor = current_floor + 1;
				}
				else
				{
					next_floor = current_floor - 1;
				}
			}
			else
			{
				if (current_floor == 10)
				{
					scan_direction = DOWN;
					current_direction = DOWN;
					next_floor = current_floor - 1;
				}
				else
				{
					next_floor = current_floor + 1;
				}
			}
		}
	}
	return 0;
}

int MoveElevator(int floor)
{
	if (floor == current_floor)
		return 0;
	else
	{
		printk("Moving floor to %d\n", floor);
		ssleep(2);
		current_floor = floor;
		return 1;
	}
}

int PassengerQueueSize(int floor)
{
	struct queue_entry* ent;
	struct list_head* pos;
	int count;
	count = 0;
	mutex_lock_interruptible(&mutex_passenger_queue);
	list_for_each(pos, &passenger_queue[floor - 1])
	{
		ent = list_entry(pos, struct queue_entry, list);
		if (ent->m_type == 2)
			count += 2;
		else
			count++;
	}
	mutex_unlock(&mutex_passenger_queue);
	//printk("Counted %d entries in queue\n", count);
	return count;
}

int PassengerQueueWeight(int floor)
{
	struct queue_entry* ent;
	struct list_head* pos;
	int weight;
	weight = 0;
	mutex_lock_interruptible(&mutex_passenger_queue);
	list_for_each(pos, &passenger_queue[floor - 1])
	{
		ent = list_entry(pos, struct queue_entry, list);
		weight += PassengerWeight(ent->m_type);
	}
	mutex_unlock(&mutex_passenger_queue);
	return weight;

}

int ElevatorListSize(void)
{
	struct queue_entry* ent;
	struct list_head* pos;
	int count;
	count = 0;
	mutex_lock_interruptible(&mutex_elevator_list);
	list_for_each(pos, &elevator_list)
	{
		ent = list_entry(pos, struct queue_entry, list);
		if (ent->m_type == 2)
			count += 2;
		else
			count++;
	}
	mutex_unlock(&mutex_elevator_list);
	return count;
}

void UnloadPassengers(void)
{
	//printk("In unload passengers!\n");
	struct queue_entry *ent;
	struct list_head *pos, *q;
	mutex_lock_interruptible(&mutex_elevator_list);
	list_for_each_safe(pos, q, &elevator_list)
	{
		ent = list_entry(pos, struct queue_entry, list);
		if (ent->m_destinationFloor == current_floor)
		{
			printk("Unloaded Passenger!\n");
			passengers_serviced++;	
			passengers_serviced_by_floor[ent->m_startingFloor-1]++;
			list_del(pos);
			kfree(ent);
		}
	}
	mutex_unlock(&mutex_elevator_list);
}

int ShouldUnload(void)
{
	//printk("In should unload!\n Current_floor = %d\nNext_floor = %d\n\n\n\n",
	//	current_floor, next_floor);
	struct queue_entry *ent;
	struct list_head *pos;
	mutex_lock_interruptible(&mutex_elevator_list);
	list_for_each(pos, &elevator_list)
	{
		ent = list_entry(pos, struct queue_entry, list);
		printk("Current Entry Destination: %d\n", ent->m_destinationFloor);
		if (ent->m_destinationFloor == current_floor)
		{
			printk("Should Unload returned true\n");
			mutex_unlock(&mutex_elevator_list);
			return 1;
		}
	}
	mutex_unlock(&mutex_elevator_list);
	return 0;
}

int ShouldLoad(void)
{
	struct queue_entry *ent;
	struct list_head *pos;
	if (ElevatorListSize() == 8)
		return 0;
	mutex_lock_interruptible(&mutex_passenger_queue);
	list_for_each(pos, &passenger_queue[current_floor-1])
	{
		ent = list_entry(pos, struct queue_entry, list);
		if ((PassengerWeight(ent->m_type) + ElevatorWeight() <= 16) &&
		    ((ent->m_destinationFloor > current_floor && scan_direction == UP) ||
		    (ent->m_destinationFloor < current_floor && scan_direction == DOWN)))
		{
			mutex_unlock(&mutex_passenger_queue);
			return 1;
		}
	}
	mutex_unlock(&mutex_passenger_queue);
	return 0;
}

int NextPickupFloor(void)
{
	int i;
	for (i = 1; i <= NUM_FLOORS; i++)
	{
		if (PassengerQueueSize(i) > 0)
			return i;
	}
	return 0;
}

int NextDropoffFloor(void)
{
	struct queue_entry *ent;
	struct list_head *pos;
	mutex_lock_interruptible(&mutex_elevator_list);
	list_for_each(pos, &elevator_list)
	{
		ent = list_entry(pos, struct queue_entry, list);
		mutex_unlock(&mutex_elevator_list);
		return ent->m_destinationFloor;
	}
	mutex_unlock(&mutex_elevator_list);
	return 0;
}

void FIFOLoadSinglePassenger(int floor)
{
	struct queue_entry *ent;
	struct list_head *pos, *q;
	int weight;
	weight = ElevatorWeight();
	mutex_lock_interruptible(&mutex_passenger_queue);
	list_for_each_safe(pos, q, &passenger_queue[floor - 1])
	{
		ent = list_entry(pos, struct queue_entry, list);
		if (ent->m_startingFloor == floor &&
		    (PassengerWeight(ent->m_type) + weight <= 16))
		{
			struct queue_entry* new_entry;
			new_entry = kmalloc(sizeof(struct queue_entry), KFLAGS);
			new_entry->m_type = ent->m_type;
			new_entry->m_startingFloor = ent->m_startingFloor;
			new_entry->m_destinationFloor = ent->m_destinationFloor;
			mutex_lock_interruptible(&mutex_elevator_list);
			list_add_tail(&new_entry->list, &elevator_list);
			list_del(pos);
			kfree(ent);
			mutex_unlock(&mutex_elevator_list);
			mutex_unlock(&mutex_passenger_queue);
			return;	
		}
	}
	mutex_unlock(&mutex_passenger_queue);
}

int ElevatorWeight(void)
{
	struct queue_entry* ent;
	struct list_head* pos;
	int weight;
	weight = 0;
	mutex_lock_interruptible(&mutex_elevator_list);
	list_for_each(pos, &elevator_list)
	{
		ent = list_entry(pos, struct queue_entry, list);
		weight += PassengerWeight(ent->m_type);
	}
	mutex_unlock(&mutex_elevator_list);
	return weight;
}
