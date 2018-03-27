#ifndef __ELEVATOR_SYSCALLS_H
#define __ELEVATOR_SYSCALLS_H

void elevator_syscalls_create(void);
void elevator_syscalls_remove(void);
void InitializeQueue(void);
void CleanupQueue(void);
void QueuePassenger(int type, int start, int dest);
void PrintQueue(void);
char* PrintQueueToString(void);
void PrintElevator(void);
int ElevatorRun(void* data);
int MoveElevator(int floor);
int PassengerQueueSize(int floor);
int PassengerQueueWeight(int floor);
int ElevatorListSize(void);
void UnloadPassengers(void);
int ShouldUnload(void);
int ShouldLoad(void);
int ElevatorWeight(void);

// FIFO implementation functions
int NextPickupFloor(void);
int NextDropoffFloor(void);
void FIFOLoadSinglePassenger(int floor);
#endif /*__ELEVATOR_SYSCALLS_H*/
