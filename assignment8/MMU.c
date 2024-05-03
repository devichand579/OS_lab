#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <limits.h>
#include <math.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/wait.h>


#define PROCESS_SEND_TYPE 10
#define MMU_TO_PROCESS 20
#define INVALID_PAGE_REF -2
#define PAGE_FAULT -1
#define PROCESS_OVER_SIGNAL -9
#define PAGE_FAULT_HANDLED_SIGNAL 5
#define TERMINATED_SIGNAL 10

int reference_count = 0;
int *page_fault_frequency;
FILE *result_file;
int i;

struct message_buffer
{
	long message_type;
	int process_id;
	int page_number;
};

struct mmu_to_process_buffer
{
	long message_type;
	int frame_number;
};

struct mmu_to_scheduler_buffer
{
	long message_type;
	char message_buffer[1];
};

typedef struct {
	int frame_number;
	int is_valid;
	int count;
} page_table_entry;

typedef struct {
	pid_t process_id;
	int m;
	int frame_count;
	int frame_allocated;
} process_control_block;

typedef struct
{
	int current;
	int free_frames[];
} free_frame_list;




key_t free_frames_key, page_table_key;
key_t message_queue_2_key, message_queue_3_key;
key_t pcb_key;
process_control_block *pcb_ptr;
page_table_entry *page_table_ptr;
free_frame_list *free_frames_ptr;

int page_table_id, free_frames_id;
int message_queue_2_id, message_queue_3_id;
int pcb_id;


int m,k;


void send_reply(int process_id, int frame_number)
{
	struct mmu_to_process_buffer buffer;
	buffer.message_type = process_id + MMU_TO_PROCESS;
	buffer.frame_number = frame_number;
	int length = sizeof(struct message_buffer) - sizeof(long);
	int result = msgsnd(message_queue_3_id, &buffer, length, 0);
	if (result == -1)
	{
		perror("msgsnd");
		exit(EXIT_FAILURE);
	}
}

void notify_scheduler(int type)
{
	struct mmu_to_scheduler_buffer buffer;
	buffer.message_type = type;
	int length = sizeof(struct message_buffer) - sizeof(long);
	int result = msgsnd(message_queue_2_id, &buffer, length, 0);
	if (result == -1)
	{
		perror("msgsnd");
		exit(EXIT_FAILURE);
	}

}

int read_request(int* process_id)
{
	struct message_buffer buffer;
	int length;
	/* The length is essentially the size of the structure minus sizeof(mtype) */
	length = sizeof(struct message_buffer) - sizeof(long);
	memset(&buffer, 0, sizeof(buffer));

	int result = msgrcv(message_queue_3_id, &buffer, length, PROCESS_SEND_TYPE, 0);
	if (result == -1)
	{
		if(errno == EINTR)
			return -1;
		perror("msgrcv");
		exit(EXIT_FAILURE);
	}
	*process_id = buffer.process_id;
	return buffer.page_number;
}


int handle_page_fault(int process_id, int page_number)
{
	int i;
	if (free_frames_ptr->current == -1 || pcb_ptr[i].frame_count <= pcb_ptr[i].frame_allocated)
	{
		int min = INT_MAX, mini = -1;
		int victim = 0;
		for (i = 0; i < pcb_ptr[i].m; i++)
		{
			if (page_table_ptr[process_id * m + i].is_valid == 1)
			{
				if (page_table_ptr[process_id * m + i].count < min)
				{
					min = page_table_ptr[process_id * m + i].count;
					victim = page_table_ptr[process_id * m + i].frame_number;
					mini = i;
				}
			}
		}
		page_table_ptr[process_id * m + mini].is_valid = 0;
		return victim;
	}
	else
	{
		int frame_number = free_frames_ptr->free_frames[free_frames_ptr->current];
		free_frames_ptr->current -= 1;
		return frame_number;
	}
}

void free_pages(int i)
{

	int k = 0;
	for (k = 0; k < pcb_ptr[i].m; i++)
	{
		if (page_table_ptr[i * m + k].is_valid == 1)
		{
			free_frames_ptr->free_frames[free_frames_ptr->current + 1] = page_table_ptr[i * m + k].frame_number;
			free_frames_ptr->current += 1;
		}
	}
}

int service_memory_request()
{
	pcb_ptr = (process_control_block*)(shmat(pcb_id, NULL, 0));
	
	page_table_ptr = (page_table_entry*)(shmat(page_table_id, NULL, 0));
	
	free_frames_ptr = (free_frame_list*)(shmat(free_frames_id, NULL, 0));
	

	int process_id = -1, page_number;
	page_number = read_request(&process_id);
	if(page_number == -1 && process_id == -1)
	{
		return 0;
	}
	int i = process_id;
	if (page_number == PROCESS_OVER_SIGNAL)
	{
		free_pages(process_id);
		notify_scheduler(TERMINATED_SIGNAL);
		return;
	}
	reference_count ++;
	printf("Page reference : (%d,%d,%d)\n",reference_count,process_id,page_number);
	fprintf(result_file,"Page reference : (%d,%d,%d)\n",reference_count,process_id,page_number);
	if (pcb_ptr[process_id].m < page_number || page_number < 0)
	{
		printf("Invalid Page Reference : (%d %d)\n",process_id,page_number);
		fprintf(result_file,"Invalid Page Reference : (%d %d)\n",process_id,page_number);
		send_reply(process_id, INVALID_PAGE_REF);
		printf("Process %d: TRYING TO ACCESS INVALID PAGE REFERENCE %d\n", process_id, page_number);
		free_pages(process_id);
		notify_scheduler(TERMINATED_SIGNAL);

	}
	else
	{
		if (page_table_ptr[i * m + page_number].is_valid == 0)
		{
			//PAGE FAULT
			printf("Page Fault : (%d, %d)\n",process_id,page_number);
			fprintf(result_file,"Page Fault : (%d, %d)\n",process_id,page_number);
			page_fault_frequency[process_id] += 1;
			send_reply(process_id, -1);
			int frame_number = handle_page_fault(process_id, page_number);
			page_table_ptr[i * m + page_number].is_valid = 1;
			page_table_ptr[i * m + page_number].count = reference_count;
			page_table_ptr[i * m + page_number].frame_number = frame_number;
			
			notify_scheduler(PAGE_FAULT_HANDLED_SIGNAL);
		}
		else
		{
			send_reply(process_id, page_table_ptr[i * m + page_number].frame_number);
			page_table_ptr[i * m + page_number].count = reference_count;
			//FRAME FOUND
		}
	}
	if(shmdt(pcb_ptr) == -1)
	{
		perror("pcb_ptr-shmdt");
		exit(EXIT_FAILURE);
	}
	if(shmdt(page_table_ptr) == -1)
	{
		perror("page_table_ptr-shmdt");
		exit(EXIT_FAILURE);
	}
	if(shmdt(free_frames_ptr) == -1)
	{
		perror("free_frames_ptr-shmdt");
		exit(EXIT_FAILURE);
	}
}
int flag = 1;
void handle_terminate_signal(int sig)
{
	flag = 0;
}

int main(int argc, char const *argv[])
{
	if (argc < 4)
	{
		printf("mmu m2key m3key ptbkey fkey pcbkey m k\n");
		exit(EXIT_FAILURE);
	}
	message_queue_2_id = atoi(argv[1]);
	message_queue_3_id = atoi(argv[2]);
	page_table_id = atoi(argv[3]);
	free_frames_id = atoi(argv[4]);
	pcb_id = atoi(argv[5]);
	m = atoi(argv[6]);
	k = atoi(argv[7]);
	signal(SIGUSR2, handle_terminate_signal);
	page_fault_frequency = (int *)malloc(k*sizeof(int));
	for(i=0;i<k;i++)
	{
		page_fault_frequency[i] = 0; 
	} 
	result_file = fopen("result.txt","w");
	while(flag)
	{
		service_memory_request();
	}
	printf("Page fault Count for each Process:\n");	
	fprintf(result_file,"Page fault Count for each Process:\n");
	printf("Process Id\tFrequency\n");
	fprintf(result_file,"Process Id\tFrequency\n");
	for(i = 0;i<k;i++)
	{
		printf("%d\t%d\n",i,page_fault_frequency[i]);
		fprintf(result_file,"%d\t%d\n",i,page_fault_frequency[i]);
	}
	fclose(result_file);
	return 0;
}
 