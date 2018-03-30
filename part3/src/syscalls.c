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
#define OFFLINE 4
#define NUM_FLOORS 10


struct qEntry {
	struct list_head list;
	int theType;
	int theStartFloor;
	int theDestFloor;
};

static struct list_head pQueue[NUM_FLOORS];
static struct list_head eList;


extern int nowDirection;  
extern int findDirection;
extern int shouldOffline;
extern int nowFloor;
extern int nextFloor;
extern int nowPass;
extern int nowWeight;
extern int waitPass;
extern int loadedPass;
extern int loadedPassFloor[NUM_FLOORS];

extern struct mutex mutex_pQueue;
extern struct mutex mutex_eList;
extern struct task_struct* eThread;

int getPassWeight(int type);

void qInitial(void)
{
	int i;
	i = 0;
	while(i < NUM_FLOORS)
	{
		INIT_LIST_HEAD(&pQueue[i]);
		i++;
	}
	
	INIT_LIST_HEAD(&eList);
}

void qClean(void)
{
	struct qEntry *myEntry;
	struct list_head *myLoc, *q;
	int i;

	mutex_lock_interruptible(&mutex_pQueue);
	
	i = 0;
	while(i < NUM_FLOORS)
	{
		list_for_each_safe(myLoc, q, &pQueue[i])
		{
			myEntry = list_entry(myLoc, struct qEntry, list);
			list_del(myLoc);
			kfree(myEntry);
		}
		i++;
	}
	

	mutex_unlock(&mutex_pQueue);
	mutex_lock_interruptible(&mutex_eList);
	list_for_each_safe(myLoc, q, &eList)
	{
		myEntry = list_entry(myLoc, struct qEntry, list);
		list_del(myLoc);
		kfree(myEntry);
	}
	mutex_unlock(&mutex_eList);
}

int getPassWeight(int type)
{
    
    switch(type){
        case 1:
            return 2;
            break;
        case 2:
            return 1;
            break;
        case 3:
            return 4;
            break;
        case 4:
            return 6;
            break;
        default:
            return 0;
            break;
    }
    
}

void qPass(int type, int start, int dest)
{
	struct qEntry *addEntry;
	addEntry = kmalloc(sizeof(struct qEntry), KFLAGS);
	addEntry->theType = type;
	addEntry->theStartFloor = start;
	addEntry->theDestFloor = dest;
	mutex_lock_interruptible(&mutex_pQueue);
	list_add_tail(&addEntry->list, &pQueue[start - 1]);
	mutex_unlock(&mutex_pQueue);
	

}


int sizeQPass(int floor)
{
    struct qEntry* myEntry;
    struct list_head* myLoc;
    int track;
    track = 0;
    mutex_lock_interruptible(&mutex_pQueue);
    list_for_each(myLoc, &pQueue[floor - 1])
    {
        myEntry = list_entry(myLoc, struct qEntry, list);
        if (myEntry->theType == 1)
            track++;
        if(myEntry->theType == 2)
            track++;
        else
            track +=2;
        
        
    }
    mutex_unlock(&mutex_pQueue);
    
    return track;
}

int weightQPass(int floor)
{
    struct qEntry* myEntry;
    struct list_head* myLoc;
    int weight;
    weight = 0;
    mutex_lock_interruptible(&mutex_pQueue);
    list_for_each(myLoc, &pQueue[floor - 1])
    {
        myEntry = list_entry(myLoc, struct qEntry, list);
        weight += getPassWeight(myEntry->theType);
    }
    mutex_unlock(&mutex_pQueue);
    return weight;
    
}


int sizeElevList(void)
{
    struct qEntry* myEntry;
    struct list_head* myLoc;
    int track;
    track = 0;
    mutex_lock_interruptible(&mutex_eList);
    list_for_each(myLoc, &eList)
    {
        myEntry = list_entry(myLoc, struct qEntry, list);
        if (myEntry->theType == 1)
            track++;
        if(myEntry->theType == 2)
            track++;
        else
            track +=2;
    }
    mutex_unlock(&mutex_eList);
    return track;
}

int getElevWeight(void)
{
    struct qEntry* myEntry;
    struct list_head* myLoc;
    int weight;
    weight = 0;
    mutex_lock_interruptible(&mutex_eList);
    list_for_each(myLoc, &eList)
    {
        myEntry = list_entry(myLoc, struct qEntry, list);
        weight += getPassWeight(myEntry->theType);
    }
    mutex_unlock(&mutex_eList);
    return weight;
}

int elevGo(void* data)
{
	while (!kthread_should_stop())
	{
		
		switch(nowDirection){
			case 0:
			{
				findDirection = UP;
				if (Load() && !shouldOffline)
					nowDirection = LOADING;
				else
				{
					nowDirection = UP;
					nextFloor = nowFloor + 1;
				}
				break;
			}
			case 1:
			{
				elevChange(nextFloor);
				if (nowFloor == 10)
				{
					findDirection = DOWN;
					nowDirection = DOWN;
				}
				if ((Load() && !shouldOffline) || Unload())
					nowDirection = LOADING;
				else if (nowFloor == 10)
				{
					nextFloor = nowFloor - 1;
				}
				else
				{
					nextFloor = nowFloor + 1;
				}
				break;
			}
			case 2:
			{
				elevChange(nextFloor);
				if (nowFloor == 1)
				{
					findDirection = UP;
					nowDirection = UP;
				}
			
				if (shouldOffline && !sizeElevList() && nowFloor == 1)
				{
					nowDirection = OFFLINE;
					shouldOffline = 0;
					findDirection = UP;
				}
				else if ((Load() &&!shouldOffline) || Unload())
					nowDirection = LOADING;
				else if (nowFloor == 1)
				{
					nextFloor = nowFloor + 1;
				}
				else
				{
					nextFloor = nowFloor - 1;
				}
				break;
			}
			case 3:
			{
				ssleep(1);
				passOff();
				while (Load() && !shouldOffline)
					loadOnePass(nowFloor);
				nowDirection = findDirection;
				if (nowDirection == DOWN)
				{
					if (nowFloor == 1)
					{
						findDirection = UP;
						nowDirection = UP;
						nextFloor = nowFloor + 1;
					}
					else
					{
						nextFloor = nowFloor - 1;
					}
				}
				else
				{
					if (nowFloor == 10)
					{
						findDirection = DOWN;
						nowDirection = DOWN;
						nextFloor = nowFloor - 1;
					}
					else
					{
						nextFloor = nowFloor + 1;
					}
				}
				break;
			}
			case 4:
			{
				break;
			}
		}
		
		
	}
	return 0;
}

int elevChange(int floor)
{

	if (floor == nowFloor)
		return 0;
	else
	{
		
		ssleep(2);
		nowFloor = floor;
		return 1;
	}
}



void passOff(void)
{
	
	struct qEntry *myEntry;
	struct list_head *myLoc, *q;
	mutex_lock_interruptible(&mutex_eList);
	list_for_each_safe(myLoc, q, &eList)
	{
		myEntry = list_entry(myLoc, struct qEntry, list);
		if (myEntry->theDestFloor == nowFloor)
		{
			
			loadedPass++;	
			loadedPassFloor[myEntry->theStartFloor-1]++;
			list_del(myLoc);
			kfree(myEntry);
		}
	}
	mutex_unlock(&mutex_eList);
}

int Unload(void)
{
	
	struct qEntry *myEntry;
	struct list_head *myLoc;
	mutex_lock_interruptible(&mutex_eList);
	list_for_each(myLoc, &eList)
	{
		myEntry = list_entry(myLoc, struct qEntry, list);
		
		if (myEntry->theDestFloor == nowFloor)
		{
			
			mutex_unlock(&mutex_eList);
			return 1;
		}
	}
	mutex_unlock(&mutex_eList);
	return 0;
}

int Load(void)
{
	struct qEntry *myEntry;
	struct list_head *myLoc;
	if (sizeElevList() == 10) 
		return 0;
	mutex_lock_interruptible(&mutex_pQueue);
	list_for_each(myLoc, &pQueue[nowFloor-1])
	{
		myEntry = list_entry(myLoc, struct qEntry, list);
		
		if ((getPassWeight(myEntry->theType) + getElevWeight() <= 15) &&
		    ((myEntry->theDestFloor > nowFloor && findDirection == UP) ||
		    (myEntry->theDestFloor < nowFloor && findDirection == DOWN)))
		{
			mutex_unlock(&mutex_pQueue);
			return 1;
		}
	}
	mutex_unlock(&mutex_pQueue);
	return 0;
}

int Pickup(void)
{
	int i;
	
	i = 0;
	while(i <= NUM_FLOORS)
	{
		if (sizeQPass(i) > 0)
			return i;
		i++;
	}
	
	return 0;
}

int Dropoff(void)
{
	struct qEntry *myEntry;
	struct list_head *myLoc;
	mutex_lock_interruptible(&mutex_eList);
	list_for_each(myLoc, &eList)
	{
		myEntry = list_entry(myLoc, struct qEntry, list);
		mutex_unlock(&mutex_eList);
		return myEntry->theDestFloor;
	}
	mutex_unlock(&mutex_eList);
	return 0;
}

void loadOnePass(int floor)
{
	struct qEntry *myEntry;
	struct list_head *myLoc, *q;
	int weight;
	weight = getElevWeight();
	mutex_lock_interruptible(&mutex_pQueue);
	list_for_each_safe(myLoc, q, &pQueue[floor - 1])
	{
		myEntry = list_entry(myLoc, struct qEntry, list);
		if (myEntry->theStartFloor == floor &&
		    (getPassWeight(myEntry->theType) + weight <= 15))
		{
			struct qEntry* addEntry;
			addEntry = kmalloc(sizeof(struct qEntry), KFLAGS);
			addEntry->theType = myEntry->theType;
			addEntry->theStartFloor = myEntry->theStartFloor;
			addEntry->theDestFloor = myEntry->theDestFloor;
			mutex_lock_interruptible(&mutex_eList);
			list_add_tail(&addEntry->list, &eList);
			list_del(myLoc);
			kfree(myEntry);
			mutex_unlock(&mutex_eList);
			mutex_unlock(&mutex_pQueue);
			return;	
		}
	}
	mutex_unlock(&mutex_pQueue);
}






extern long (*STUB_start_elevator)(void);
long start_elevator(void) {
    
    if (nowDirection == OFFLINE)
    {
        
        nowDirection = IDLE;
        return 0;
    }
    else if(shouldOffline)
    {
        shouldOffline = 0;
        return 0;
    }
    else
    {
        return 1;
    }
    
    
}

extern long (*STUB_issue_request)(int,int,int);
long issue_request(int passenger_type, int start_floor, int destination_floor) {
    
    
    
    if ( (passenger_type > 4 || passenger_type < 1) || start_floor < 1 || destination_floor > 10 )
    {
        return 1;
    }
    qPass(passenger_type, start_floor, destination_floor);
    
    
    return 0;
}



extern long (*STUB_stop_elevator)(void);
long stop_elevator(void) {
    
    if (shouldOffline == 1)
        return 1;
    shouldOffline = 1;
    return 0;
}


void elevator_syscalls_create(void) {
    
    STUB_issue_request = &(issue_request);
    STUB_start_elevator = &(start_elevator);
    STUB_stop_elevator = &(stop_elevator);
}

void elevator_syscalls_remove(void) {
    
    STUB_issue_request = NULL;
    STUB_start_elevator = NULL;
    STUB_stop_elevator = NULL;
}
