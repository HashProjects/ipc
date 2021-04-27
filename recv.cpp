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

/* The ids for the shared memory segment */
int shmid;

/* The pointer to the shared memory */
void *sharedMemPtr;

/* The name of the received file */
const char recvFileName[] = "recvfile";

/* The PID of the sender */
pid_t sendPid = -1;

/* Set of signals to wait for from receiver */
sigset_t sigWatchlist;

pid_t getSenderPid(const int *shmId);

int receiveMsgSize();

void cleanUp(const int& shmid, void* sharedMemPtr);

/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory 
 * @param sharedMemPtr - the pointer to the shared memory
 */
void init(int& shmid, void*& sharedMemPtr)
{
	/* Get a unique key by using a file called keyfile.txt containing string 	
	   and call ftok("keyfile.txt", 'a') in order to generate the key.
	 */
	key_t key;
	key = ftok("keyfile.txt", 'a');
	if (key == -1) {
		fprintf(stderr, "Failed to generate key: %s\n", strerror(errno));
		exit(-1);
	}
	
	/* Allocate a piece of shared memory. The size of the segment must be SHARED_MEMORY_CHUNK_SIZE. */
	shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0666 | IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr, "Failed to obtain shared memory: %s\n", strerror(errno));
		exit(-1);
	}

	/* Attach to the shared memory */
	sharedMemPtr = shmat(shmid, NULL, 0);
	if (sharedMemPtr == (void*)-1) {
		fprintf(stderr, "Failed to obtain shared memory pointer: %s\n", strerror(errno));
		cleanUp(shmid, sharedMemPtr);
		exit(-1);
	}

	/* Get PID of the sender */
	sendPid = getSenderPid(&shmid);
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
		cleanUp(shmid, sharedMemPtr);
		exit(-1);
	}
	
    /* Receive the signal and get the message size. If the size is not 0, 
	 * then we copy that many bytes from the shared
     * memory region to the file. Otherwise, if 0, then we close the file and
     * exit.
     *
     * NOTE: the received file will always be saved into the file called
     * "recvfile"
     */

	/* Number of bytes from sender */
	int msgSize;

	int blockCounter = 1;
	int fileSizeCounter = 0;

	fprintf(stdout, "Waiting for file transfer to begin...\r");
	fflush(stdout);

	/* Keep receiving until the sender set the size to 0, indicating that
 	 * there is no more data to send.
 	 */	
	while ((msgSize = receiveMsgSize()) != 0) {
		/* Save the shared memory to file */
		if (fwrite(sharedMemPtr, sizeof(char), msgSize, fp) < 0) {
			perror("fwrite");
			cleanUp(shmid, sharedMemPtr);
			break;
		}

		fileSizeCounter += msgSize;
		fprintf(stdout, "Reading block %d (%d bytes transferred)\r", blockCounter++, fileSizeCounter);
		fflush(stdout);

		/* Tell the sender that we are ready for the next file chunk
		 * by sending a SIGUSR2 signal.
		 */
		if (kill(sendPid, SIGUSR2) == -1) {
			fprintf(stderr, "Failed to signal sender: %s\n", strerror(errno));
			cleanUp(shmid, sharedMemPtr);
			exit(-1);
		}
	}
	
	fprintf(stdout, "File transfer complete (%d bytes)       \n", fileSizeCounter);
	
	/* Close the file */
	fclose(fp);
}

/**
 * Perfoms the cleanup functions
 * @param shmid - the id of the shared memory segment
 * @param sharedMemPtr - the pointer to the shared memory
 */
void cleanUp(const int& shmid, void* sharedMemPtr)
{
	/* Detach from shared memory */
	if (shmdt(sharedMemPtr) == -1) {
		fprintf(stderr, "Failed to detach shared memory: %s\n", strerror(errno));
	}
	/* Deallocate the shared memory chunk */
	if (shmctl(shmid, IPC_RMID, NULL) == -1) {
		fprintf(stderr, "Failed to deallocate shared memory: %s\n", strerror(errno));
	}
}

/**
 * Gets the PID of the sender
 * @param shmId - The ID of the shared memory segment
 */
pid_t getSenderPid(const int* shmId) {
	/* The PID of this process */
	pid_t selfPid = getpid();
	/* Info on shared memory segment */
	shmid_ds shmInfo;
	
	// Wait for sender to run and attach to shared memory
	// Since recv runs and attaches first, lpid contains recv (self) PID.
	do {
		// Read info on shared memory segment
		shmctl(*shmId, IPC_STAT, &shmInfo);
		sleep(1);
	} while (shmInfo.shm_lpid == selfPid);
	// When sender attaches, lpid changes to sender PID and breaks loop.
	return shmInfo.shm_lpid;
}

/**
 * Extracts number of bytes sent from sender
 */
int receiveMsgSize() {
	// Data struct that was sent along with signal
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
void ctrlCSignal(int signal) {
	fprintf(stdout, "File transfer canceled.                    \n");
	fflush(stdout);
	/* Free system V resources */
	cleanUp(shmid, sharedMemPtr);
	exit(0);
}

int main(int argc, char** argv) {
	fprintf(stdout, "recv - receives data from a sender\n");

	/* Overide the default signal handler for the
	 * SIGINT signal with signalHandlerFunc
	 */
	signal(SIGINT, ctrlCSignal); 

	// Populate the set of signals to watch from sender
	// SIGUSR1 = sender put some data in shared memory
	sigemptyset(&sigWatchlist);
	if (sigaddset(&sigWatchlist, SIGUSR1) == -1) {
		fprintf(stderr, "Failed to add signal mask: %s\n", strerror(errno));
		exit(-1);
	}

	// Block SIGUSR1 from terminating process (default behavior)
	if (sigprocmask(SIG_SETMASK, &sigWatchlist, NULL) == -1) {
		fprintf(stderr, "Failed to set signals mask: %s\n", strerror(errno));
		exit(-1);
	}

	/* Initialize */
	init(shmid, sharedMemPtr);
	
	/* Go to the main loop */
	mainLoop(); 

	/* Detach from shared memory segment and deallocate shared memory */
	cleanUp(shmid, sharedMemPtr);

	return 0;
}
