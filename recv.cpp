#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <cerrno>
#include "msg.h"    /* For the message struct */

/* The size of the shared memory chunk */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The ids for the shared memory segment and the message queue */
int shmid, msqid;

/* The pointer to the shared memory */
void *sharedMemPtr;

/* The name of the received file */
const char recvFileName[] = "recvfile";

/* The PID of the sender */
pid_t sendpid;

void cleanUp(const int& shmid, const int& msqid, void* sharedMemPtr);

/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory 
 * @param msqid - the id of the shared memory
 * @param sharedMemPtr - the pointer to the shared memory
 */

void init(int& shmid, int& msqid, void*& sharedMemPtr)
{
	
	/* TODO: 1. Create a file called keyfile.txt containing string "Hello world" (you may do
 		    so manually or from the code).
	         2. Use ftok("keyfile.txt", 'a') in order to generate the key.
		 3. Use the key in the TODO's below. Use the same key for the queue
		    and the shared memory segment. This also serves to illustrate the difference
		    between the key and the id used in message queues and shared memory. The id
		    for any System V object (i.e. message queues, shared memory, and sempahores) 
		    is unique system-wide among all System V objects. Two objects, on the other hand,
		    may have the same key.
	 */
	
	key_t key;
	key = ftok("keyfile.txt", 'a');
	if (key == -1) {
		fprintf(stderr, "Failed to generate key: %s\n", strerror(errno));
		exit(-1);
	}
	
	/* TODO: Allocate a piece of shared memory. The size of the segment must be SHARED_MEMORY_CHUNK_SIZE. */
	shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0666 | IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr, "failed to obtain shared memory: %s\n", strerror(errno));
		exit(-1);
	}
	
	// Get PID of the sender. Sender performed the last shared
	// memory operation so its PID is stored in shm_lpid
	shmid_ds *shmInfo = (shmid_ds*) malloc(sizeof(shmid_ds));
	shmctl(shmid, IPC_STAT, shmInfo);
	sendpid = shmInfo->shm_lpid;
	free(shmInfo);
	printf("init pid=%d ppid=%d\n", getpid(), getppid());
	printf("cpid=%d lpid=%d\n", shmInfo->shm_cpid, shmInfo->shm_lpid);

	/* TODO: Attach to the shared memory */
	sharedMemPtr = shmat(shmid, NULL, 0);

	/* TODO: Create a message queue */
	msqid = msgget(key, 0666 | IPC_CREAT);

	/* Store the IDs and the pointer to the shared memory region in the corresponding parameters */
}

/**
 * The main loop
 */
void mainLoop()
{
	
	/* Open the file for writing */
	FILE* fp = fopen(recvFileName, "w");
		
	/* Error checks */
	if(!fp)
	{
		fprintf(stderr, "failed to open file for received data: %s\n", recvFileName);	
		cleanUp(shmid, msqid, sharedMemPtr);
		exit(-1);
	}
		
    /* TODO: Receive the signal and get the message size. If the size is not 0, 
	 * then we copy that many bytes from the shared
     * memory region to the file. Otherwise, if 0, then we close the file and
     * exit.
     *
     * NOTE: the received file will always be saved into the file called
     * "recvfile"
     */
	/* The size of the mesage */
	int msgSize = 0;

	struct sigaction *act;
	act = (struct sigaction*) malloc (sizeof(struct sigaction));
	//act -> sa_sigaction = handler;
	act -> sa_flags = SA_SIGINFO;
	
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR1);


	message msg;

	int result = msgrcv(msqid, &msg, sizeof(msg), SENDER_DATA_TYPE, 0);
	if (result != -1) {
		fprintf(stdout, "message received %d bytes (memory size to read: %d)\n", result, msg.size);
	} else {
		fprintf(stderr, "message receive failed: %s\n", strerror(errno));
	}
	msgSize = msg.size;




	/* Keep receiving until the sender set the size to 0, indicating that
 	 * there is no more data to send
 	 */	
	while(msgSize != 0)
	{	
		/* If the sender is not telling us that we are done, then get to work */
		if(msgSize != 0)
		{
			/* Save the shared memory to file */
			if(fwrite(sharedMemPtr, sizeof(char), msgSize, fp) < 0)
			{
				perror("fwrite");
				cleanUp(shmid, msqid, sharedMemPtr);
				break;
			}
			
			/* TODO: Tell the sender that we are ready for the next file chunk. 
 			 * I.e. send a message of type RECV_DONE_TYPE (the value of size field
 			 * does not matter in this case). 
 			 */
			msg.mtype = RECV_DONE_TYPE;
			
			result = msgsnd(msqid, &msg, sizeof(msg), 0);
			if (result != -1) {
				fprintf(stdout, "message sent: RECV_DONE_TYPE\n");
			} else {
				fprintf(stderr, "message sent failure: %s\n", strerror(errno));
				break;
			}

			result = msgrcv(msqid, &msg, sizeof(msg), SENDER_DATA_TYPE, 0);
			
			if (result != -1) {
				fprintf(stdout, "message received %d bytes (memory size: %d)\n", result, msg.size);
			} else {
				fprintf(stderr, "message receive failure: %s\n", strerror(errno));
				break;
			}
			msgSize = msg.size;
		}
		/* We are done */
		else
		{

		}
	}
	
	/* Close the file */
	fprintf(stdout, "Transfer complete. Closing file\n");
	fclose(fp);
}



/**
 * Perfoms the cleanup functions
 * @param sharedMemPtr - the pointer to the shared memory
 * @param shmid - the id of the shared memory segment
 * @param msqid - the id of the message queue
 */

void cleanUp(const int& shmid, const int& msqid, void* sharedMemPtr)
{
	/* TODO: Detach from shared memory */
	int result = shmdt(sharedMemPtr);
	if (result == -1) {
		fprintf(stderr, "Failed to detach shared memory: %s\n", strerror(errno));
	}
	/* TODO: Deallocate the shared memory chunk */
	result = shmctl(shmid, IPC_RMID, NULL);
	if (result == -1) {
		fprintf(stderr, "Failed to deallocate the shared memory: %s\n", strerror(errno));
	}
	/* TODO: Deallocate the message queue */
	result = msgctl(msqid, IPC_RMID, NULL);
	if (result == -1) {
		fprintf(stderr, "Failed to deallocate message queue: %s\n", strerror(errno));
	}
}

/**
 * Handles the exit signal
 * @param signal - the signal type
 */

void ctrlCSignal(int signal)
{
	/* Free system V resources */
	cleanUp(shmid, msqid, sharedMemPtr);
	exit(0);
}

int main(int argc, char** argv)
{
	fprintf(stdout, "%s: waiting for a message...\n", argv[0]);	
	/* TODO: Install a singnal handler (see signaldemo.cpp sample file).
 	 * In a case user presses Ctrl-c your program should delete message
 	 * queues and shared memory before exiting. You may add the cleaning functionality
 	 * in ctrlCSignal().
 	 */
	
	/* Overide the default signal handler for the
	 * SIGINT signal with signalHandlerFunc
	 */
	signal(SIGINT, ctrlCSignal); 
				
	/* Initialize */
	init(shmid, msqid, sharedMemPtr);
	
	/* Go to the main loop */
	mainLoop(); 

	/** TODO: Detach from shared memory segment, and deallocate shared memory and message queue (i.e. call cleanup) **/
	cleanUp(shmid, msqid, sharedMemPtr);	
	return 0;
}
