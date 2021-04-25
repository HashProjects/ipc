
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include "msg.h"    /* For the message struct */

/* The size of the shared memory chunk */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The ids for the shared memory segment and the message queue */
int shmid, msqid;

/* The pointer to the shared memory */
void* sharedMemPtr;

// use signals
bool useSignals = false;
sigset_t sigs;
union sigval *sigdata = 0;

/* The PID of the receiver */
pid_t recvPid;

void cleanUp(const int& shmid, const int& msqid, void* sharedMemPtr);


void handlerSIGUSR2(int signo, siginfo_t *info, void *context) {
	//msgSize = info->_sifields._rt.si_sigval.sival_int;
	fprintf(stdout, "signal handler called");
}

/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory 
 * @param msqid - the id of the shared memory
 */

void init(int& shmid, int& msqid, void*& sharedMemPtr)
{
	/* Get a unique key by using a file called keyfile.txt containing string 	
	   and call ftok("keyfile.txt", 'b') in order to generate the key.
	 */
	key_t key;
	key = ftok("keyfile.txt", 'b');
	if (key == -1) {
		fprintf(stderr, "Failed to generate key: %s\n", strerror(errno));
		exit(-1);
	}

	/* Get the id of the shared memory segment. The size of the segment is SHARED_MEMORY_CHUNK_SIZE */
	/* obtain the identifier of a previously created shared memory segment 
	   (when shmflg is zero and key does not have the value IPC_PRIVATE)
	*/
	
	shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0666 | IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr, "failed to obtain shared memory: %s\n", strerror(errno));
		exit(-1);
	}
	/* Obtain the shared memory pointer for the memory segment */
	sharedMemPtr = shmat(shmid, NULL, IPC_CREAT);
	if (sharedMemPtr == (void*)-1) {
		fprintf(stderr, "failed to obtain shared memory pointer: %s\n", strerror(errno));
		exit(-1);
	}
	
	
	/* Attach to the message queue */
	if (!useSignals) {
		msqid = msgget(key, 0666 | IPC_CREAT);
		if (msqid == -1) {
			fprintf(stderr, "failed to obtain message queue: %s\n", strerror(errno));
			cleanUp(shmid, msqid, sharedMemPtr);
			exit(-1);
		}
	} else {
			// Set of signals to wait for from sender
		// SIGUSR1 = read from shared memory
		sigemptyset(&sigs);
		sigaddset(&sigs, SIGUSR2);
		sigdata = (union sigval*)malloc(sizeof(union sigval));

		shmid_ds *shmInfo = (shmid_ds*) malloc(sizeof(shmid_ds));
		shmctl(shmid, IPC_STAT, shmInfo);
		recvPid = shmInfo->shm_cpid;
		printf("selfpid=%d cpid=%d lpid=%d\n", getpid(), shmInfo->shm_cpid, shmInfo->shm_lpid);
		free(shmInfo);

		struct sigaction *act = (struct sigaction*)malloc(sizeof(struct sigaction));
		act->sa_sigaction = handlerSIGUSR2;
		act->sa_flags = SA_SIGINFO;
		sigaction(SIGUSR2, act, NULL);
		free(act);
	}
}

/**
 * Performs the cleanup functions
 * @param sharedMemPtr - the pointer to the shared memory
 * @param shmid - the id of the shared memory segment
 * @param msqid - the id of the message queue
 */

void cleanUp(const int& shmid, const int& msqid, void* sharedMemPtr)
{
	/* Detach from shared memory
	   recv will clean up all resource after file transfer is finished
	 */
	int result = shmdt(sharedMemPtr);
	if (result == -1) {
		fprintf(stderr, "failed to detach shared memory: %s\n", strerror(errno));
	}

	free(sigdata);
}

void ctrlCSignal(int signal)
{
	/* Free system V resources */
	cleanUp(shmid, msqid, sharedMemPtr);
	exit(0);
}

/**
 * The main send function
 * @param fileName - the name of the file
 */
void send(const char* fileName)
{
	/* Open the file for reading */
	FILE* fp = fopen(fileName, "r");

	struct stat statbuf;
	int sentFileSize = 0;

	int result = 0; // most recent error code
	bool waiting = true;

	/* A buffer to store message we will send to the receiver. */
	message sndMsg; 
	
	/* A buffer to store message received from the receiver. */
	message rcvMsg;
	
	/* Was the file open? */
	if(!fp)
	{
  		fprintf(stderr, "File does not exist or is not accessible: %s\n", fileName);
		cleanUp(shmid, msqid, sharedMemPtr);
		exit(-1);
	}

	// get file information since the file size is required
	if (stat(fileName, &statbuf) == -1) {
  		// This error case may not be possible, since fopen probably will fail first
		fprintf(stderr, "File does not exist or is not accessible: %s\n", fileName);
		fclose(fp);
		return;
	}
	// display the file name
	fprintf(stdout, "Sending %s\n", fileName);

	
	/* Read the whole file */
	while(!feof(fp))
	{
		/* Read at most SHARED_MEMORY_CHUNK_SIZE from the file and store them in shared memory. 
 		 * fread will return how many bytes it has actually read (since the last chunk may be less
 		 * than SHARED_MEMORY_CHUNK_SIZE).
 		 */
		if((sndMsg.size = fread(sharedMemPtr, sizeof(char), SHARED_MEMORY_CHUNK_SIZE, fp)) < 0)
		{
			perror("failed to read from file. Was the receiver process killed?\n");
			cleanUp(shmid, msqid, sharedMemPtr);
			fclose(fp);
			exit(-1);
		}

		// Report the file transfer status to stdout 
		sentFileSize += sndMsg.size;
		fprintf(stdout, "File transfer: %.2lf%% complete. %s                                 \r", 
			sentFileSize * 100.0 /statbuf.st_size, (waiting ? " Waiting for receiver..." : ""));
		fflush(stdout);


		if (useSignals) {
			sigdata->sival_int = sndMsg.size;
			if (sigqueue(recvPid, SIGUSR1, *sigdata) != 0) {
				fprintf(stderr, "Failed to signal receiver. %s\n", strerror(errno));
				cleanUp(shmid, msqid, sharedMemPtr);
				exit(-1);
			}

			int signal;
			if (sigwait(&sigs, &signal) != 0) {
				fprintf(stderr, "Failed to receive signal from sender. %s\n", strerror(errno));
				cleanUp(shmid, msqid, sharedMemPtr);
				exit(-1);
			}
		} else {
			/* Send a message to the receiver telling him that the data is ready 
			* (message of type SENDER_DATA_TYPE) 
			*/
			sndMsg.mtype = SENDER_DATA_TYPE;
			result = msgsnd(msqid, &sndMsg, sizeof(sndMsg), 0);
			if (result == -1) {
				fprintf(stderr, "failed to send message to receiver: Was the receiver process killed?\n");
				break;
			}
			/* Wait until the receiver sends us a message of type RECV_DONE_TYPE telling us 
			* that he finished saving the memory chunk. 
			*/ 
			result = msgrcv(msqid, &rcvMsg, sizeof(rcvMsg), RECV_DONE_TYPE, 0);
			if (result == -1) {
				fprintf(stderr, "failed to receive message from receiver: Was the receiver process killed?\n");
				break;
			}
		}
		waiting = false; // no longer waiting for the receiver to start reading data

	}
	
	if (result != -1) {
		/** once we are out of the above loop, we have finished sending the file.
		 * Lets tell the receiver that we have nothing more to send. We will do this by
		 * sending a message of type SENDER_DATA_TYPE with size field set to 0.
		 * 
		 * This is only done if there have been no errors previously. 	
		 */ 

		sndMsg.mtype = SENDER_DATA_TYPE;
		sndMsg.size = 0;

		result = msgsnd(msqid, &sndMsg, sizeof(sndMsg), 0);
		if (result != -1) {
			fprintf(stdout, "File transfer complete (%d bytes)                    \n", sentFileSize);
		} else {
			fprintf(stdout, "Sending message to receiver failed: Was the receiver process killed?\n");
		}
	} else {
		fprintf(stdout, "File transfer failed\n");
	}
	/* Close the file */
	fclose(fp);	 
}


int main(int argc, char** argv)
{
	
	/* Check the command line arguments */
	if(argc < 2)
	{
		fprintf(stdout, "send - sends data to a receiver\n");
		fprintf(stderr, "USAGE: %s <FILE NAME>\n", argv[0]);
		exit(-1);
	}

	if (argc > 2) {
		if (strcmp(argv[2], "signals") == 0) {
			useSignals = true;
		}
	}

	signal(SIGINT, ctrlCSignal);
		
	/* Connect to shared memory and the message queue */
	init(shmid, msqid, sharedMemPtr);
	
	/* Send the file */
	send(argv[1]);
	
	/* Cleanup */
	cleanUp(shmid, msqid, sharedMemPtr);
		
	return 0;
}
