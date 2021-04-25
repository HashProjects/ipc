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
int shmid;//, msqid;

/* The pointer to the shared memory */
void *sharedMemPtr;

/* The name of the received file */
const char recvFileName[] = "recvfile";

/* Number of bytes from sender */
int msgSize;

/* Set of signals to wait for from receiver */
sigset_t sigWatchlist;

pid_t getSenderPid(const int *shmId);

int receiveMsgSize();

void cleanUp(const int& shmid, void* sharedMemPtr);


/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory 
 * @param msqid - the id of the shared memory
 * @param sharedMemPtr - the pointer to the shared memory
 */
void init(int& shmid, void*& sharedMemPtr)
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

	/* TODO: Attach to the shared memory */
	sharedMemPtr = shmat(shmid, NULL, 0);

	/* TODO: Create a message queue */
	//msqid = msgget(key, 0666 | IPC_CREAT);

	/* Store the IDs and the pointer to the shared memory region in the corresponding parameters */
}

/**
 * The main loop
 */
void mainLoop()
{
	/* Get PID of the sender */
	pid_t sendPid = getSenderPid(&shmid);

	/* Open the file for writing */
	FILE* fp = fopen(recvFileName, "w");
		
	/* Error checks */
	if(!fp)
	{
		fprintf(stderr, "failed to open file for received data: %s\n", recvFileName);	
		cleanUp(shmid, sharedMemPtr);
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

	//printf("msgsize=%d\n", msgSize);

	/* Keep receiving until the sender set the size to 0, indicating that
 	 * there is no more data to send
 	 */	
	while((msgSize = receiveMsgSize()) != 0)
	{	
		/* Save the shared memory to file */
		if(fwrite(sharedMemPtr, sizeof(char), msgSize, fp) < 0)
		{
			perror("fwrite");
			cleanUp(shmid, sharedMemPtr);
			break;
		}
		
		/* TODO: Tell the sender that we are ready for the next file chunk
		 * by sending a SIGUSR2 signal.
		 */
		kill(sendPid, SIGUSR2);
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
void cleanUp(const int& shmid, void* sharedMemPtr)
{
	/* TODO: Detach from shared memory */
	if (shmdt(sharedMemPtr) == -1) {
		fprintf(stderr, "Failed to detach shared memory: %s\n", strerror(errno));
	}
	/* TODO: Deallocate the shared memory chunk */
	if (shmctl(shmid, IPC_RMID, NULL) == -1) {
		fprintf(stderr, "Failed to deallocate the shared memory: %s\n", strerror(errno));
	}
}

/**
 * Gets the PID of the sender
 * Must be called after the sender performed an operation on
 * the shared memory segment, such as attaching, and before any other
 * process performs an operation on it.
 * 
 * @param shmId - The ID of the shared memory segment
 */
pid_t getSenderPid(const int* shmId) {
	/* The PID of this process */
	pid_t selfPid = getpid();
	/* The PID of the sender */
	pid_t sendPid = -1;
	/* Info on shared memory segment */
	shmid_ds shmInfo;

	// Wait for sender to run and attach to shared memory
	while (sendPid == -1) {
		// Read info on shared memory segment
		shmctl(*shmId, IPC_STAT, &shmInfo);
		// Find PID of sender: when sender attaches,
		// its PID is stored in shm_lpid
		if (shmInfo.shm_lpid != selfPid) {
			sendPid = shmInfo.shm_lpid;
		}
		sleep(1);
	}
	printf("selfpid=%d sendpid=%d\n", selfPid, sendPid);
	return sendPid;
}

int receiveMsgSize() {
	// Data struct sent along with signal
	siginfo_t sigInfo;
	// Wait for signal from sender
	sigwaitinfo(&sigWatchlist, &sigInfo);
	// Extract number of bytes sent
	return sigInfo._sifields._rt.si_sigval.sival_int;
}

/**
 * Handles the exit signal
 * @param signal - the signal type
 */
void ctrlCSignal(int signal)
{
	/* Free system V resources */
	cleanUp(shmid, sharedMemPtr);
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

	// Populate the set of signals to watch from sender
	// SIGUSR1 = read from shared memory
	sigemptyset(&sigWatchlist);
	sigaddset(&sigWatchlist, SIGUSR1);

	// By default, processes terminate on receiving SIGUSR1
	// Need to block SIGUSR1
	sigprocmask(SIG_SETMASK, &sigWatchlist, NULL);

	/* Initialize */
	init(shmid, sharedMemPtr);
	
	/* Go to the main loop */
	mainLoop(); 

	/** TODO: Detach from shared memory segment, and deallocate shared memory and message queue (i.e. call cleanup) **/
	cleanUp(shmid, sharedMemPtr);

	return 0;
}
