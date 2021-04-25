CPSC 351 Section 3
Project: IPC


Group Members:
Thanh Vuong - tnvuong@csu.fullerton.edu
Eric Britten - ebritten@csu.fullerton.edu
Ian Patterson -ipatterson2@csu.fullerton.edu
Nolan O'Donnell - nolan794@csu.fullerton.edu

Programming Language:
C++

Environment:
Ubuntu 20 / Tuffix

How to Compile:
make

How to Run:

Run each command in a different terminal.  The order that they are started in does not matter.
./recv
./send <filename>

An 18KB file called datafile.dat has been included. An example send command is ./send datafile.dat

Once both finish, this command can be used to see if file contents match 
diff <filename> recvfile  # check if the file contents match

Extra Credit:
<maybe>

Test Cases and Screenshots
See Design document

Collaboration
The team held meetings on Discord and used Visual Studio Code Live Share for 
collaborative development.  Additionally, group chat was used to communicate
while offline as well as Google Documents.  GitHub was used to manage the code.

Contributions:
Thanh Vuong
* Error handling
* Signal handling research

Eric Britten
* Ran the collorative live share to generate basic functionality
* Test cases / Screenshots

Ian Patteerson
* Error handling

Nolan O'Donnell
* Charts and Pseudocode