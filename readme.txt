CPSC 351 Section 3
Project: IPC


GROUP MEMBERS:
Thanh Vuong - tnvuong@csu.fullerton.edu
Eric Britten - ebritten@csu.fullerton.edu
Ian Patterson - ipatterson2@csu.fullerton.edu
Nolan O'Donnell - nolan794@csu.fullerton.edu

PROGRAMMING LANGUAGE:
C++

ENVIRONMENT:
Ubuntu 20 / Tuffix

HOW TO COMPILE:
Open a terminal, navigate to location of send.cpp, recv.cpp, and makefile, then type
make

HOW TO RUN:
For the version using message queue, either send or recv can start first.
For the version using signals, receiver MUST be run before sender.
Open a terminal, navigate to location of recv, then type
./recv
Open another terminal, navigate to location of send, then type
./send <filename>

The version that handles signals is in the signals folder and can be run with these commands:
signals/recv
signals/send <filename>

An 18KB file called datafile.dat has been included. An example send command is 
./send datafile.dat

Once both finish, this command can be used to see if file contents match 
diff <filename> recvfile

EXTRA CREDIT:
Implemented. Please see details in design documentation.

TEST CASES AND SCREENSHOTS
See Design document.

COLLABORATION:
The team held meetings on Discord and used Visual Studio Code Live Share for 
live coding together (similar to Google Docs). Additionally, group chat was used 
to communicate while offline as well as Google Documents. GitHub was used to manage the code.

CONTRIBUTIONS:
Thanh Vuong
* Research, implementation, and documentation of extra credit
* Helped scheduling team meetings on when2meet
* Error handling
* Testing
* Live coding

Eric Britten
* Ran the collorative live share to generate basic functionality
* Test cases and Screenshots
* Testing
* Live coding

Ian Patterson
* Error handling
* Testing
* Live coding

Nolan O'Donnell
* Charts and Pseudocode
* Testing
* Live coding