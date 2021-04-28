
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>

/* The size of the shared memory chunk */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The id for the shared memory segment */
int shmid;

/* The pointer to the shared memory */
void* sharedMemPtr;

/* The PID of the receiver */
pid_t recvPid;

/* Set of signals to wait for from receiver */
sigset_t sigWatchlist;

pid_t getRecvPid(const int *shmId);

void cleanUp(const int& shmid, void* sharedMemPtr);

/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory 
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

	/* Get the id of the shared memory segment. The size of the segment must be SHARED_MEMORY_CHUNK_SIZE */
	/* obtain the identifier of a previously created shared memory segment 
	   (when shmflg is zero and key does not have the value IPC_PRIVATE)
	*/
	shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0666 | IPC_CREAT);
	if (shmid == -1) {
		fprintf(stderr, "Failed to obtain shared memory: %s\n", strerror(errno));
		exit(-1);
	}

	// Get PID of the receiver
	recvPid = getRecvPid(&shmid);
	
	// Attach to shared memory and obtain pointer to it
	sharedMemPtr = shmat(shmid, NULL, IPC_CREAT);
	if (sharedMemPtr == (void*)-1) {
		fprintf(stderr, "Failed to obtain shared memory pointer: %s\n", strerror(errno));
		exit(-1);
	}
}

/**
 * Performs the cleanup functions
 * @param sharedMemPtr - the pointer to the shared memory
 * @param shmid - the id of the shared memory segment
 */
void cleanUp(const int& shmid, void* sharedMemPtr)
{
	/* Detach from shared memory */
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
	struct stat statbuf;
	int sentFileSize = 0;

	//int result = 0; // most recent error code
	bool waiting = true;

	/* Open the file for reading */
	FILE* fp = fopen(fileName, "r");
	
	/* Was the file open? */
	if(!fp)
	{
		perror("fopen");
		cleanUp(shmid, sharedMemPtr);
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

	// Data struct to be sent along with signal
	union sigval sigData;

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
			cleanUp(shmid, sharedMemPtr);
			fclose(fp);
			exit(-1);
		}

		// Report the file transfer status to stdout 
		sentFileSize += bytesRead;
		fprintf(stdout, "File transfer: %.2lf%%. %s\n", 
			sentFileSize * 100.0 /statbuf.st_size, (waiting ? " Waiting for receiver..." : ""));
		fflush(stdout);

		/* Send a SIGUSR1 signal to the receiver telling him that the data is ready 
 		 * and number of bytes sent in shared memory
 		 */
		sigData.sival_int = bytesRead;
        
		if (sigqueue(recvPid, SIGUSR1, sigData) != 0) {
			fprintf(stderr, "Failed to signal receiver. %s                   \n", strerror(errno));
			cleanUp(shmid, sharedMemPtr);
			exit(-1);
		}
        fprintf(stdout, "Sent SIGUSR1 to recv (%d bytes). ", bytesRead);
        fflush(stdout);
		/* Wait until the receiver sends us a signal SIGUSR2 telling us 
 		 * that he finished saving the memory chunk. 
 		 */
		int signal;
		if (sigwait(&sigWatchlist, &signal) != 0) {
			fprintf(stderr, "Failed to receive signal from receiver. %s\n", strerror(errno));
			cleanUp(shmid, sharedMemPtr);
			exit(-1);
		}
        fprintf(stdout, "Received SIGUSR2 from recv. ");
        fflush(stdout);
		waiting = false; // no longer waiting for the receiver to start reading data
	}
	
	/** Once we are out of the above loop, we have finished sending the file.
 	  * Lets tell the receiver that we have nothing more to send. We will do this by
 	  * sending a SIGUSR1 signal with value field set to 0. 	
	  */
	fprintf(stdout, "\nFile transfer complete (%d bytes)\n", sentFileSize);sigData.sival_int = 0;
	sigData.sival_int = 0;
	if (sigqueue(recvPid, SIGUSR1, sigData) != 0) {
		fprintf(stderr, "Sending message to receiver failed: Was the receiver process killed?\n");
		cleanUp(shmid, sharedMemPtr);
		exit(-1);
	}
	
	/* Close the file */
	fclose(fp);
}

/**
 * Gets the PID of the receiver.
 * Must be called after the receiver performed an operation on
 * the shared memory segment, such as attaching, and before any other
 * process performs any operation on it.
 * @param shmId - the ID of a shared memory segment
 */
pid_t getRecvPid(const int *shmId) {
	// Read info on shared memory segment
	shmid_ds shmInfo;
	shmctl(*shmId, IPC_STAT, &shmInfo);

	// Receiver performed the last shared
	// memory operation so its PID is stored in shm_lpid
	return shmInfo.shm_lpid;
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
	fprintf(stdout, "send - sends data to a receiver\n");

	/* Check the command line arguments */
	if (argc < 2) {
		fprintf(stderr, "USAGE: %s <FILE NAME>\n", argv[0]);
		exit(-1);
	}

	// register Ctrl+C handler
	signal(SIGINT, ctrlCSignal);

	// Populate the set of signals to watch from receiver
	// SIGUSR2 = receiver ready to receive more
	sigemptyset(&sigWatchlist);
	sigaddset(&sigWatchlist, SIGUSR2);

	// Block SIGUSR2 from terminating process (default behavior)
	sigprocmask(SIG_SETMASK, &sigWatchlist, NULL);

	/* Connect to shared memory */
	init(shmid, sharedMemPtr);

	/* Send the file */
	send(argv[1]);

	/* Cleanup */
	cleanUp(shmid, sharedMemPtr);

	return 0;
}
