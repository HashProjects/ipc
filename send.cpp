
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "msg.h"    /* For the message struct */
#include <signal.h>

/* The size of the shared memory chunk */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The ids for the shared memory segment and the message queue */
int shmid, msqid;

/* The pointer to the shared memory */
void* sharedMemPtr;

void cleanUp(const int& shmid, const int& msqid, void* sharedMemPtr);
/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory 
 * @param msqid - the id of the shared memory
 */

void init(int& shmid, int& msqid, void*& sharedMemPtr)
{
	/* TODO: 
        1. Create a file called keyfile.txt containing string "Hello world" (you may do
 		    so manually or from the code).
	    2. Use ftok("keyfile.txt", 'a') in order to generate the key.
		3. Use the key in the TODO's below. Use the same key for the queue
		    and the shared memory segment. This also serves to illustrate the difference
		    between the key and the id used in message queues and shared memory. The id
		    for any System V objest (i.e. message queues, shared memory, and sempahores) 
		    is unique system-wide among all SYstem V objects. Two objects, on the other hand,
		    may have the same key.
	 */
	key_t key;
	key = ftok("keyfile.txt", 'a');
	if (key == -1) {
		fprintf(stderr, "Failed to generate key: %s\n", strerror(errno));
		exit(-1);
	}

	/* TODO: Get the id of the shared memory segment. The size of the segment must be SHARED_MEMORY_CHUNK_SIZE */
	/* obtain the identifier of a previously created shared memory segment 
	   (when shmflg is zero and key does not have the value IPC_PRIVATE)
	*/
	
	shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0666 | IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr, "Failed to obtain shared memory: %s\n", strerror(errno));
		exit(-1);
	}

	sharedMemPtr = shmat(shmid, NULL, IPC_CREAT);
	if (sharedMemPtr == (void*)-1) {
		fprintf(stderr, "Failed to obtain shared memory pointer: %s\n", strerror(errno));
		exit(-1);
	}
	
	/* TODO: Attach to the message queue */
	// msqid = msgget(key, 0666 | IPC_CREAT);
	// if (msqid == -1) {
	// 	fprintf(stderr, "failed to obtain message queue: %s\n", strerror(errno));
	// 	cleanUp(shmid, msqid, sharedMemPtr);
	// 	exit(-1);
	// }
	/* Store the IDs and the pointer to the shared memory region in the corresponding parameters */
	
	// shmid_ds *shmInfo = (shmid_ds*) malloc(sizeof(shmid_ds));
	// shmctl(shmid, IPC_STAT, shmInfo);
	// printf("init pid=%d ppid=%d\n", getpid(), getppid());
	// printf("cpid=%d lpid=%d\n", shmInfo->shm_cpid, shmInfo->shm_lpid);

}

/**
 * Performs the cleanup functions
 * @param sharedMemPtr - the pointer to the shared memory
 * @param shmid - the id of the shared memory segment
 * @param msqid - the id of the message queue
 */

void cleanUp(const int& shmid, const int& msqid, void* sharedMemPtr)
{
	/* TODO: Detach from shared memory */
	if (shmdt(sharedMemPtr) == -1) {
		fprintf(stderr, "Failed to detach shared memory: %s\n", strerror(errno));
	}
}

/**
 * The main send function
 * @param fileName - the name of the file
 */
void send(const char* fileName)
{
	/* The PID of this process */
	pid_t selfpid = getpid();
	/* The PID of the receiver */
	pid_t recvpid = -1;
	/* Info on shared memory segment */
	shmid_ds *shmInfo = (shmid_ds*) malloc(sizeof(shmid_ds));

	// Wait for receiver to run and attach to shared memory
	do {
		// Read info on shared memory segment
		shmctl(shmid, IPC_STAT, shmInfo);
		//fprintf(stdout, "self= %dcpid=%d lpid=%d", selfpid, shmInfo->shm_cpid, shmInfo->shm_lpid);
		// Find PID of receiver: when receiver attaches,
		// its PID is stored in shm_lpid
		if (shmInfo->shm_lpid != selfpid) {
			recvpid = shmInfo->shm_lpid;
		}
		sleep(1);
	} while (recvpid == -1);
	free(shmInfo);
	printf("recvpid=%d\n", recvpid);

	// Set of signals to wait for from receiver
	// SIGUSR2 = ready to receive more
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGUSR2);

	/* Open the file for reading */
	FILE* fp = fopen(fileName, "r");
	
	/* A buffer to store message we will send to the receiver. */
	// message sndMsg; 
	
	/* A buffer to store message received from the receiver. */
	// message rcvMsg;
	
	/* Was the file open? */
	if(!fp)
	{
		perror("fopen");
		cleanUp(shmid, msqid, sharedMemPtr);
		exit(-1);
	}

	int bytesRead;
	/* Read the whole file */
	while(!feof(fp))
	{
		/* Read at most SHARED_MEMORY_CHUNK_SIZE from the file and store them in shared memory. 
 		 * fread will return how many bytes it has actually read (since the last chunk may be less
 		 * than SHARED_MEMORY_CHUNK_SIZE).
 		 */
		bytesRead = fread(sharedMemPtr, sizeof(char), SHARED_MEMORY_CHUNK_SIZE, fp);
		if (bytesRead < 0) {
			perror("failed to read from file\n");
			cleanUp(shmid, msqid, sharedMemPtr);
			fclose(fp);
			exit(-1);
		}

		/* TODO: Send a signal SIGUSR1 to the receiver telling him that the data is ready 
 		 * and number of bytes sent in shared memory
 		 */
		fprintf(stdout, "Sending signal to receiver: %d bytes ready to read\n", bytesRead);
		union sigval *sigdata = (union sigval*)malloc(sizeof(union sigval));
		sigdata->sival_int = bytesRead;
		if (sigqueue(recvpid, SIGUSR1, *sigdata) != 0) {
			fprintf(stderr, "Failed to signal receiver. %s\n", strerror(errno));
			cleanUp(shmid, msqid, sharedMemPtr);
			exit(-1);
		}

		/* TODO: Wait until the receiver sends us a signal SIGUSR2 telling us 
 		 * that he finished saving the memory chunk. 
 		 */
		int signal;
		if (sigwait(&sigs, &signal) != 0) {
			fprintf(stderr, "Failed to receive signal from sender. %s\n", strerror(errno));
			cleanUp(shmid, msqid, sharedMemPtr);
			exit(-1);
		}
	}
	
	/** TODO: once we are out of the above loop, we have finished sending the file.
 	  * Lets tell the receiver that we have nothing more to send. We will do this by
 	  * sending a message of type SENDER_DATA_TYPE with size field set to 0. 	
	  */
	fprintf(stdout, "Finished.\n");
	bytesRead = 0;
	if (sigqueue(recvpid, SIGUSR1, sigval {bytesRead}) != 0) {
		fprintf(stderr, "Failed to signal receiver. %s\n", strerror(errno));
		cleanUp(shmid, msqid, sharedMemPtr);
		exit(-1);
	}
	
	/* Close the file */
	fclose(fp);
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
	signal(SIGINT, ctrlCSignal);

	/* Check the command line arguments */
	fprintf(stdout, "send - sends data to a receiver\n");
	if(argc < 2)
	{
		fprintf(stderr, "USAGE: %s <FILE NAME>\n", argv[0]);
		exit(-1);
	}

	fprintf(stdout, "sending %s\n", argv[1]);
		
	/* Connect to shared memory and the message queue */
	init(shmid, msqid, sharedMemPtr);
	
	/* Send the file */
	send(argv[1]);
	
	/* Cleanup */
	cleanUp(shmid, msqid, sharedMemPtr);
		
	return 0;
}
