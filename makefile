# the compiler: gcc for C program, define as g++ for C++
  CC = g++
  RM = rm

  # compiler flags:
  #  -g    adds debugging information to the executable file
  #  -Wall turns on most, but not all, compiler warnings
  CFLAGS  = -g -Wall

  # the build target executable:
  SEND = send
  RECV = recv

  all: send recv sends recvs

  send : send.cpp
	g++ -g -Wall -o send send.cpp

  recv : recv.cpp
	g++ -g -Wall -o recv recv.cpp

  sends : signals/send.cpp
	g++ -g -Wall -o signals/send signals/send.cpp

  recvs : signals/recv.cpp
	g++ -g -Wall -o signals/recv signals/recv.cpp

  clean:
	rm send recv
