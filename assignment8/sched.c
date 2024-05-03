#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/msg.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <ctype.h>
#include <sys/shm.h>


#define MAX_BUFFER_SIZE 100
#define MAX_PAGES 1000
#define MAX_PROCESSES 1000
#define FROM_PROCESS_MSG_TYPE 10
#define TO_PROCESS_MSG_TYPE 20  
#define FROM_MMU_MSG_TYPE 20

#define PAGE_FAULT_HANDLED_SIGNAL 5
#define TERMINATED_SIGNAL 10

int num_processes; // Number of processes
typedef struct _mmu_to_scheduler_msg {
	long    message_type;
	char message_buffer[1];
} mmu_to_scheduler_msg;


typedef struct mymsgbuf {
	long    message_type;
	int process_id;
} mymsgbuf;


int send_message( int queue_id, struct mymsgbuf *message_buffer )
{
	int     result, length;

	/* The length is essentially the size of the structure minus sizeof(mtype) */
	length = sizeof(struct mymsgbuf) - sizeof(long);

	if ((result = msgsnd(queue_id, message_buffer, length, 0)) == -1)
	{
		perror("Error in sending message");
		exit(EXIT_FAILURE);
	}

	return result;
}

int read_message( int queue_id, long type, struct mymsgbuf *message_buffer )
{
	int     result, length;

	/* The length is essentially the size of the structure minus sizeof(mtype) */
	length = sizeof(struct mymsgbuf) - sizeof(long);

	if ((result = msgrcv(queue_id, message_buffer, length, type,  0)) == -1)
	{
		perror("Error in receiving message");
		exit(EXIT_FAILURE);
	}

	return result;
}

int read_message_mmu( int queue_id, long type, mmu_to_scheduler_msg *message_buffer )
{
	int result, length;

	/* The length is essentially the size of the structure minus sizeof(mtype) */
	length = sizeof(mmu_to_scheduler_msg) - sizeof(long);

	if ((result = msgrcv(queue_id, message_buffer, length, type,  0)) == -1)
	{
		perror("Error in receiving message");
		exit(EXIT_FAILURE);
	}

	return result;
}
int main(int argc , char * argv[])
{
	int mq1_key, mq2_key, master_pid;
	if (argc < 5) {
		printf("Usage: Scheduler mq1_key mq2_key k master_pid\n");
		exit(EXIT_FAILURE);
	}
	mq1_key = atoi(argv[1]);
	mq2_key = atoi(argv[2]);
	num_processes = atoi(argv[3]);
	master_pid = atoi(argv[4]);

	mymsgbuf msg_send, msg_recv;

	int mq1 = msgget(mq1_key, 0666);
	int mq2 = msgget(mq2_key, 0666);
	if (mq1 == -1)
	{
		perror("Message Queue1 creation failed");
		exit(EXIT_FAILURE);
	}
	if (mq2 == -1)
	{
		perror("Message Queue2 creation failed");
		exit(EXIT_FAILURE);
	}
	printf("Total Number of Processes received = %d\n", num_processes);

	// Initialize variables for running array
	int terminated_processes = 0; // To keep track of running processes to exit at last
	while (1)
	{
		read_message(mq1, FROM_PROCESS_MSG_TYPE, &msg_recv);
		int current_process_id = msg_recv.process_id;

		msg_send.message_type = TO_PROCESS_MSG_TYPE + current_process_id;
		send_message(mq1, &msg_send);

		// Receive messages from MMU
		mmu_to_scheduler_msg mmu_recv;
		read_message_mmu(mq2, 0, &mmu_recv);
		if (mmu_recv.message_type == PAGE_FAULT_HANDLED_SIGNAL)
		{
			msg_send.message_type = FROM_PROCESS_MSG_TYPE;
			msg_send.process_id = current_process_id;
			send_message(mq1, &msg_send);
		}
		else if (mmu_recv.message_type == TERMINATED_SIGNAL)
		{
			terminated_processes++;
		}
		else
		{
			perror("Wrong message from MMU\n");
			exit(EXIT_FAILURE);
		}
		if (terminated_processes == num_processes)
			break;
	}
	kill(master_pid, SIGUSR1);
	pause();
	printf("Scheduler terminating ...\n") ;
	exit(EXIT_SUCCESS) ;
}
