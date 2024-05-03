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
#define TO_SCHEDULER_MSG_TYPE 10
#define FROM_SCHEDULER_MSG_TYPE 20 
#define TO_MMU_MSG_TYPE 10
#define FROM_MMU_MSG_TYPE 20 

int page_numbers[MAX_PAGES];
int num_of_pages;

typedef struct mmu_send_message_buffer {
	long    message_type;          /* Message type */
	int process_id;
	int page_number;
} mmu_send_message_buffer;

typedef struct mmu_receive_message_buffer {
	long    message_type;          /* Message type */
	int frame_number;
} mmu_receive_message_buffer;

typedef struct mymsgbuf {
	long    message_type;          /* Message type */
	int process_id;
} mymsgbuf;

void convert_reference_pg_no(char * refs)
{
	const char s[2] = "|";
	char *token;
	token = strtok(refs, s);
	while ( token != NULL )
	{
		page_numbers[num_of_pages] = atoi(token);
		num_of_pages++;
		token = strtok(NULL, s);
	}
}

int send_message_mmu( int qid, struct mmu_send_message_buffer *message_buffer )
{
	int     result, length;

	/* The length is essentially the size of the structure minus sizeof(mtype) */
	length = sizeof(struct mmu_send_message_buffer) - sizeof(long);

	if ((result = msgsnd( qid, message_buffer, length, 0)) == -1)
	{
		perror("Error in sending message");
		exit(EXIT_FAILURE);
	}

	return result;
}
int read_message_mmu( int qid, long type, struct mmu_receive_message_buffer *message_buffer )
{
	int     result, length;

	/* The length is essentially the size of the structure minus sizeof(mtype) */
	length = sizeof(struct mmu_receive_message_buffer) - sizeof(long);

	if ((result = msgrcv( qid, message_buffer, length, type,  0)) == -1)
	{
		perror("Error in receiving message");
		exit(EXIT_FAILURE);
	}

	return result;
}

int send_message( int qid, struct mymsgbuf *message_buffer )
{
	int     result, length;

	/* The length is essentially the size of the structure minus sizeof(mtype) */
	length = sizeof(struct mymsgbuf) - sizeof(long);

	if ((result = msgsnd( qid, message_buffer, length, 0)) == -1)
	{
		perror("Error in sending message");
		exit(EXIT_FAILURE);
	}

	return result;
}
int read_message( int qid, long type, struct mymsgbuf *message_buffer )
{
	int     result, length;

	/* The length is essentially the size of the structure minus sizeof(mtype) */
	length = sizeof(struct mymsgbuf) - sizeof(long);

	if ((result = msgrcv( qid, message_buffer, length, type,  0)) == -1)
	{
		perror("Error in receiving message");
		exit(EXIT_FAILURE);
	}

	return result;
}

int main(int argc, char *argv[])
{
	if (argc < 5)
	{
		perror("Please provide 5 arguments {id, mq1, mq3, ref_string}\n");
		exit(EXIT_FAILURE);
	}
	int id, mq1_key, mq3_key;
	id = atoi(argv[1]);
	mq1_key = atoi(argv[2]);
	mq3_key = atoi(argv[3]);
	num_of_pages = 0;
	convert_reference_pg_no(argv[4]);
	int mq1, mq3;
	mq1 = msgget(mq1_key, 0666);
	mq3 = msgget(mq3_key, 0666);
	if (mq1 == -1)
	{
		perror("Message Queue1 creation failed");
		exit(EXIT_FAILURE);
	}
	if (mq3 == -1)
	{
		perror("Message Queue3 creation failed");
		exit(EXIT_FAILURE);
	}
	printf("Process ID: %d\n", id);

	// Sending to scheduler
	mymsgbuf msg_send;
	msg_send.message_type = TO_SCHEDULER_MSG_TYPE;
	msg_send.process_id = id;
	send_message(mq1, &msg_send);

	// Wait until message received from scheduler
	mymsgbuf msg_recv;
	read_message(mq1, FROM_SCHEDULER_MSG_TYPE + id, &msg_recv);

	mmu_send_message_buffer mmu_send;
	mmu_receive_message_buffer mmu_recv;
	int current_page_index = 0; // Counter for page number array
	while (current_page_index < num_of_pages)
	{
		// Sending message to MMU with the page number
		printf("Sent request for page number %d\n", page_numbers[current_page_index]);
		mmu_send.message_type = TO_MMU_MSG_TYPE;
		mmu_send.process_id = id;
		mmu_send.page_number = page_numbers[current_page_index];
		send_message_mmu(mq3, &mmu_send);

		read_message_mmu(mq3, FROM_MMU_MSG_TYPE + id, &mmu_recv);
		if (mmu_recv.frame_number >= 0)
		{
			printf("Frame number from MMU received for process %d: %d\n" , id, mmu_recv.frame_number);
			current_page_index++;
		}
		else if (mmu_recv.frame_number == -1) // Here current_page_index will not be incremented
		{
			printf("Page fault occurred for process %d\n", id);
		}
		else if (mmu_recv.frame_number == -2)
		{
			printf("Invalid page reference for process %d. Terminating ...\n", id);
			exit(EXIT_FAILURE);
		}
	}
	printf("Process %d terminated successfully\n", id);
	mmu_send.page_number = -9;
	mmu_send.process_id = id;
	mmu_send.message_type = TO_MMU_MSG_TYPE;
	send_message_mmu(mq3, &mmu_send);

	exit(EXIT_SUCCESS);
	return 0;
}
