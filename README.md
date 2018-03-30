Team members:
Holly Culver
Abigail Perry
Hunter Davis


Version of Linux:
4.14.12


How to compile:
Compile with the makefile for the given part by simply typing make inside of each directory (part1, my_xtime, or part3) to run each individually. Each directory contains its own Makefile for that specific part of the assignment. 


Known bugs: 
1) At first, we were only able to run ./producer.x in part 3, elevator3test after compiling it 3 separate times before it would successfully output Return 0 stating that it has successfully worked, with this being our main first error, we found that the reason this was happening was from a mistake inside of our issue_request function. 
2) We have a known issue of inconsistency with our ./producer.x when testing the 4th provided elevator test file. This portion has inconsistent errors and runs from 10000000 to 10 and it outputs correct information; however, the portion runs extremely slow and seems to get stuck on one of the last function calls inside of producer.c


Contents of TAR:
Along with the Readme the tar file contains our directories for part1, my_xtime, and part3. Each of these directories contains their specific Makefile, along with all necessary source code to run each of these. Our alloted syscalls.h files will be within the seen include directory in part3. 

Part 1 includes a makefile, and part1.c
Part 2 includes its makefile and my_xtime.c.
Part 3 includes a src directory with: module.c, sys_issue_request.c, sys_start_elevator.c, sys_stop_elevator.c, syscalls.c
An include directory with: syscalls.h and a Makefile in the ~/ of part3. 


Special considerations:
There are no special considerations 

