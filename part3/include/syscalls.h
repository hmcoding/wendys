#ifndef __ELEVATOR_SYSCALLS_H
#define __ELEVATOR_SYSCALLS_H

void elevator_syscalls_create(void);
void elevator_syscalls_remove(void);
void qInitial(void);
void qClean(void);
void qPass(int type, int start, int dest);
int elevGo(void* data);
int elevChange(int floor);
int sizeQPass(int floor);
int weightQPass(int floor);
int sizeElevList(void);
void passOff(void);
int Unload(void);
int Load(void);
int getElevWeight(void);
int Pickup(void);
int Dropoff(void);
void loadOnePass(int floor);
#endif /*__ELEVATOR_SYSCALLS_H*/
