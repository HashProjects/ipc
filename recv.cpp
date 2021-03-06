#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
//#include <cerror>
#include "msg.h"    /* For the message struct */


/* The size of the shared memory chunk */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The ids for the shared memory segment and the message queue */
int shmid = -1, msqid = -1;

/* The pointer to the shared memory */
void *sharedMemPtr = (void*)-1;

/* The name of the received file */
const char recvFileName[] = "recvfile";

void cleanUp(const int& shmid, const int& msqid, void* sharedMemPtr);


/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory 
 * @param msqid - the id of the shared memory
 * @param sharedMemPtr - the pointer to the shared memory
 */

void init(int& shmid, int& msqid, void*& sharedMemPtr)
{
	
	/*  1. Create a file called keyfile.txt containing string "Hello world" (you may do
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
	
	/* Allocate a piece of shared memory with size SHARED_MEMORY_CHUNK_SIZE. */
	shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0666 | IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr, "failed to obtain shared memory: %s\n", strerror(errno));
		cleanUp(shmid, msqid, sharedMemPtr);
		exit(-1);
	}
	
	/* Attach to the shared memory */
	sharedMemPtr = shmat(shmid, NULL, 0);
	if (sharedMemPtr == (void*)-1) {
		fprintf(stderr, "failed to obtain shared memory pointer: %s\n", strerror(errno));
		cleanUp(shmid, msqid, sharedMemPtr);
		exit(-1);
	}
	/* Create the message queue */
	msqid = msgget(key, 0666 | IPC_CREAT);
	if (msqid == -1) {
		fprintf(stderr, "failed to obtain message queue: %s\n", strerror(errno));
		cleanUp(shmid, msqid, sharedMemPtr);
		exit(-1);
	}
	/* Store the IDs and the pointer to the shared memory region in the corresponding parameters */
	
}
 

/**
 * The main loop
 */
void mainLoop()
{
	/* The size of the mesage */
	int msgSize = 0;
	
	/* Open the file for writing */
	FILE* fp = fopen(recvFileName, "w");
		
	/* Error checks */
	if(!fp)
	{
		fprintf(stderr, "failed to open file for received data: %s\n", recvFileName);	
		cleanUp(shmid, msqid, sharedMemPtr);
		exit(-1);
	}
		
    /* Receive the first message and get the message size. The message will 
     * contain regular information. The message will be of SENDER_DATA_TYPE
     * (the macro SENDER_DATA_TYPE is defined in msg.h).  If the size field
     * of the message is not 0, then we copy that many bytes from the shared
     * memory region to the file. Otherwise, if 0, then we close the file and
     * exit.
     *
     * NOTE: the received file will always be saved into the file called
     * "recvfile"
     */
	message msg;
	int blockCounter = 1;
	int fileSizeCounter = 0;

	fprintf(stdout, "Waiting for file transfer to begin...\n");
	fflush(stdout);

	int result = msgrcv(msqid, &msg, sizeof(msg), SENDER_DATA_TYPE, 0);
	fprintf(stdout, "Received message from send for %d bytes. ", msg.size);
	if (result != -1) {
		// Report the status of the file transfer
		fileSizeCounter += msg.size;
		fprintf(stdout, "Reading block %d (%d bytes transferred)\n", blockCounter++, fileSizeCounter);
		fflush(stdout);
	} else {
		fprintf(stderr, "message receive failed: %s\n", strerror(errno));
		// for this error, the while loop will be skipped and the file will be closed
	}
	msgSize = msg.size;

	/* Keep receiving until the sender set the size to 0, indicating that
 	 * there is no more data to send
 	 */	

	while(msgSize != 0)
	{	

		/* Save the shared memory to file */
		if((result = fwrite(sharedMemPtr, sizeof(char), msgSize, fp)) < 0)
		{
			fprintf(stderr, "writing to file failure: %s\n", strerror(errno));
			cleanUp(shmid, msqid, sharedMemPtr);
			break;
		}
		
		/* Tell the sender that we are ready for the next file chunk. 
			* I.e. send a message of type RECV_DONE_TYPE (the value of size field
			* does not matter in this case). 
			*/
		msg.mtype = RECV_DONE_TYPE;
		fprintf(stdout, "Sending message to send. ");
		result = msgsnd(msqid, &msg, sizeof(msg), 0);
		if (result == -1) {
			fprintf(stderr, "message sent failure: %s\n", strerror(errno));
			break;
		}

		// Wait for the next message from the sender regarding the next block 
		// in the file that is being transferred
		result = msgrcv(msqid, &msg, sizeof(msg), SENDER_DATA_TYPE, 0);
		fprintf(stdout, "Received message from send for %d bytes. ", msg.size);
		if (result != -1) {
			// Receive was successful, report status to the console
			// This update will allow the next update to replace this one with \r
			// instead of \n
			fileSizeCounter += msg.size;
			fprintf(stdout, "Reading block %d (%d bytes transferred)\n", blockCounter++, fileSizeCounter);
			fflush(stdout);
		} else {
			fprintf(stderr, "message receive failure: %s\n", strerror(errno));
			break;
		}
		msgSize = msg.size;
	
	}
	
	// report to the output that the file transfer is complete or has failed
	if (result != -1) {
		fprintf(stdout, "File transfer complete (%d bytes)       \n", fileSizeCounter);
	} else {
		fprintf(stdout, "File transfer failed.                   \n");
	}
	/* Close the file */
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
	/* Detach from shared memory */
	int result = shmdt(sharedMemPtr);
	if (result == -1) {
		fprintf(stderr, "Failed to detach shared memory: %s\n", strerror(errno));
	}
	/* Deallocate the shared memory chunk */
	result = shmctl(shmid, IPC_RMID, NULL);
	if (result == -1) {
		fprintf(stderr, "Failed to deallocate the shared memory: %s\n", strerror(errno));
	}
	/* Deallocate the message queue */
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
	fprintf(stdout, "File transfer canceled.                    \n");
	fflush(stdout);
	cleanUp(shmid, msqid, sharedMemPtr);
	exit(0);
}

int main(int argc, char** argv)
{	
	/* Overide the default signal handler for the
	 * SIGINT signal with signalHandlerFunc
	 */
	signal(SIGINT, ctrlCSignal); 
				
	/* Initialize */
	init(shmid, msqid, sharedMemPtr);
	
	/* Go to the main loop */
	mainLoop(); 

	/* Detach from shared memory segment, and deallocate shared memory and message queue (i.e. call cleanup) **/
	cleanUp(shmid, msqid, sharedMemPtr);	
	return 0;
}
