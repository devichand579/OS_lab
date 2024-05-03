#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "event.h"

#define REPORTER 0
#define PATIENT 1
#define SALES_REP 2

char *visitor_names[] = {"Reporter", "Patient", "Sales Representative"};
int waiting_visitor[3];
int maximum_visitor[3];
int visitor_count[3];
int current_time=0, last_time=0;
int sessiondone=0, exit_print=0;
pthread_barrier_t init_barrier,doc_barrier,done_doc,done_vis;
pthread_mutex_t mutex;
pthread_cond_t cond_vis[3],cond_doc;
// Struct to hold visitor information
typedef struct {
    int type;      // Visitor type: 'P' for patient, 'R' for reporter, 'S' for sales representative
    int number;     // Visitor number
    int arrival;    // Arrival time
    int duration;
} VisitorInfo;

eventQ events;

// Function prototypes
void* doctor(void* arg);
void* visitor(void* arg);
int checksession();
void get_sync(pthread_barrier_t *barrier);
void printarrivaltime(event e);
int get_type(char type);
void create_thread(event e);
void check_create_thread(event e);
void signal_doctor();
void signal_visitor(int type);


int main() {
    // Initialization
    waiting_visitor[REPORTER] = 0;
    waiting_visitor[PATIENT] = 0;
    waiting_visitor[SALES_REP] = 0;

    maximum_visitor[REPORTER] = 1e9;
    maximum_visitor[PATIENT] = 25;
    maximum_visitor[SALES_REP] = 3;

    visitor_count[REPORTER] = 1;
    visitor_count[PATIENT] = 1;
    visitor_count[SALES_REP] = 1;

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_trylock(&mutex);
    pthread_mutex_unlock(&mutex);

    pthread_cond_init(&cond_doc, NULL);

    for (int i = 0; i < 3; i++) {
        pthread_cond_init(&cond_vis[i], NULL);
    }
    // Initialize event queue
    events = initEQ("arrival.txt");

    // Initialize threads
    pthread_t doctor_thread;
    pthread_attr_t doctor_attr;
    pthread_attr_init(&doctor_attr);
    pthread_attr_setdetachstate(&doctor_attr, PTHREAD_CREATE_JOINABLE);

    // Create doctor thread
    pthread_barrier_init(&doc_barrier, NULL, 2); // Initialize the barrier
    pthread_create(&doctor_thread, NULL, doctor, NULL);
    get_sync(&doc_barrier); // Wait for the doctor to be ready
    pthread_barrier_destroy(&doc_barrier); // Destroy the barrier

    while(1){
        while(!emptyQ(events)){
            event e = nextevent(events);
            if(e.time<= last_time){
                printarrivaltime(e);
                check_create_thread(e);
            }
            else{
                break;
            }
            events = delevent(events);
        }

        sessiondone = checksession();
        if(sessiondone && !exit_print){
            current_time = last_time;
            signal_doctor();
            exit_print = 1;

        }
        if(waiting_visitor[REPORTER] > 0){
            current_time = last_time;

            waiting_visitor[REPORTER]--;
            signal_doctor();
            signal_visitor(REPORTER);
        }
        else if (waiting_visitor[PATIENT] > 0){
            current_time = last_time;

            waiting_visitor[PATIENT]--;
            signal_doctor();
            signal_visitor(PATIENT);
        }
        else if (waiting_visitor[SALES_REP] > 0){
            current_time = last_time;

            waiting_visitor[SALES_REP]--;
            signal_doctor();
            signal_visitor(SALES_REP);
        }
        else{
            if(emptyQ(events)){
                break;
            }
            event e = nextevent(events);
            last_time = e.time;
        }
    }
    if(sessiondone && exit_print){
      current_time = last_time;
        signal_doctor();
        exit_print = 1;
    }
    return 0;
}

int checksession(){
    if( visitor_count[PATIENT] > maximum_visitor[PATIENT] && visitor_count[SALES_REP] > maximum_visitor[SALES_REP] && waiting_visitor[PATIENT] == 0 && waiting_visitor[SALES_REP] == 0 && waiting_visitor[REPORTER] == 0){
        return 1;
    }
    return 0;
}

void create_thread(event e){
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    VisitorInfo *visitor_info = (VisitorInfo*)malloc(sizeof(VisitorInfo));
    visitor_info->type = get_type(e.type);
    visitor_info->number = visitor_count[visitor_info->type];
    visitor_info->arrival = e.time;
    visitor_info->duration = e.duration;
    
    pthread_barrier_init(&init_barrier, NULL, 2); // Initialize the barrier

    visitor_count[visitor_info->type]++;
    waiting_visitor[visitor_info->type]++;
    pthread_create(&tid, &attr, visitor, (void*)visitor_info);
    get_sync(&init_barrier); // Wait for the visitor to be ready
    pthread_barrier_destroy(&init_barrier); // Destroy the barrier
}

void check_create_thread(event e){
   int type = get_type(e.type);
   char *mssg[]= {"QUOTA FULL","SESSION OVER"};
   int select = 0;

   if(!exit_print && visitor_count[type] <= maximum_visitor[type]){
       if(e.time > last_time){
           last_time = e.time;
       }
       create_thread(e);
       return;
   }
   if(exit_print){
    select = 1;
   }
   int time = e.time + 540;
    printf("[%02d:%02d%s] %s leaves (%s)\n", time / 60, time % 60, time < 720 ? "am" : "pm", visitor_names[type], mssg[select]);
    visitor_count[type]++;
}

int get_type(char type){
    if(type == 'R'){
        return REPORTER;
    }
    else if(type == 'P'){
        return PATIENT;
    }
    else if(type == 'S'){
        return SALES_REP;
    }
    return -1;
}

void signal_doctor(){
   pthread_barrier_init(&doc_barrier, NULL, 2); // Initialize the barrier
    pthread_cond_signal(&cond_doc);

    get_sync(&doc_barrier); // Wait for the doctor to be ready
    pthread_barrier_destroy(&doc_barrier); // Destroy the barrier
}

void signal_visitor(int type){
    pthread_barrier_init(&done_vis, NULL, 2); // Initialize the barrier
    pthread_cond_signal(&cond_vis[type]);
    pthread_barrier_wait(&done_vis);
    pthread_barrier_destroy(&done_vis);
}

void printarrivaltime(event e){
    int time = e.time + 540;
    printf("[%02d:%02d%s] %s arrives\n", time / 60, time % 60, time < 720 ? "am" : "pm", visitor_names[get_type(e.type)]);
}

void get_sync(pthread_barrier_t *barrier) {
    pthread_barrier_wait(barrier);
    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&mutex);
}

void* doctor(void* arg) {
   while(1){
        pthread_mutex_lock(&mutex);
        pthread_barrier_wait(&doc_barrier);
        pthread_cond_wait(&cond_doc, &mutex);
        if(sessiondone){
            int time = current_time+ 540;
            printf("[%02d:%02d%s] Doctor leaves\n", time / 60, time % 60, time < 720 ? "am" : "pm");
            pthread_mutex_unlock(&mutex);
            break;
        }
        int time = current_time+ 540;
        printf("[%02d:%02d%s] Doctor has next visitor\n", time / 60, time % 60, time < 720 ? "am" : "pm");
        pthread_mutex_unlock(&mutex);
    }
    pthread_barrier_wait(&doc_barrier);
    pthread_exit(NULL);
   }


void* visitor(void* arg) {
    VisitorInfo *visitor_info = (VisitorInfo*)arg;
    int type = visitor_info->type;
    int number = visitor_info->number;
    int duration = visitor_info->duration;

    pthread_mutex_lock(&mutex);
    pthread_barrier_wait(&init_barrier);
    pthread_cond_wait(&cond_vis[type], &mutex);

    last_time = current_time + duration;
    int time = current_time + 540;
    int end_time = last_time + 540;
    printf("[%02d:%02d%s]-[%02d:%02d%s] %s %d is in doctor's chamber\n", time / 60, time % 60, time < 720 ? "am" : "pm", end_time / 60, end_time % 60, end_time < 720 ? "am" : "pm", visitor_names[type], number);\
    pthread_mutex_unlock(&mutex);
    pthread_barrier_wait(&done_vis);
    pthread_exit(NULL);
}
