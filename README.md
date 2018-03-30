Team members:
Holly Culver
Abigail Perry
Hunter Davis

Division of Labor:
We took on every part of this project as a team.

Version of Linux:
4.14.12


How to compile:
Part 1 is compiled using make then the command $ strace -o log ./part1.x.
For my_xtime and part3, compile with the makefile for the given part by simply typing make inside of each directory (my_xtime or part3) to run each individually. Each directory contains its own Makefile for that specific part of the assignment. 


Known bugs: 
1) At first, we were only able to run ./producer.x in part 3, elevator3test after compiling it 3 separate times before it would successfully work, with this being our main first error, we found that the reason this was happening was from a mistake inside of our issue_request function. After changing the issue_request function, the producer.c test seems to be successful.
2) We have a known issue of inconsistency with our ./producer.x when testing the 4th provided elevator test file. This portion runs extremely slow and seems to get stuck on one of the last function calls inside of producer.c. We tried adding some print functions in producer.c to figure out what exactly it is getting stuck on and it seems to be one of the last lines of the file where time_sleep function is called and within that sleep and usleep is called.


Contents of TAR:
Along with the Readme the tar file contains our directories for part1, my_xtime, and part3. Each of these directories contains their specific Makefile, along with all necessary source code to run each of these. Our alloted syscalls.h files will be within the seen include directory in part3. 

-Part 1 includes a makefile, and part1.c
-my_xtime includes its makefile and my_xtime.c.
-Part 3 includes a src directory with: module.c, sys_issue_request.c, sys_start_elevator.c, sys_stop_elevator.c, syscalls.c
and an include directory with: syscalls.h and a Makefile in the ~/ of part3.
Makefile, syscalls.h, and syscall_64.tbl are the core kernel files that were modified for adding system calls for part 3.


Special considerations:
There are no special considerations 

