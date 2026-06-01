#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mpi.h>
#include <stdbool.h>
#include <math.h>

#define INITIAL_TASKS 100
#define MAX_TASKS 2000
#define MAX_SEND  10
#define MAX_ITERATIONS 3

#define TAG_REQUEST 0
#define TAG_COUNT 1
#define TAG_DATA 2

#define REQ_WORK 1
#define REQ_STOP -1

#define BASE_WEIGHT 10000000
#define DELTA_WEIGHT 10000000

#define EXIT_FAILURE 1

typedef struct {
    int repeatNum;
} Task;

typedef struct {
    Task tasks[MAX_TASKS];
    int count;
    int next_index;
    pthread_mutex_t mutex;
    pthread_cond_t need_work_cond;
    pthread_cond_t work_ready_cond;
} TaskList;

typedef struct {
    TaskList task_list;
    int rank;
    int size;
    int need_work;
    int request_in_progress;
    int requester_should_stop;
    double local_result;
    long processed_tasks;
    long received_tasks;
    long sent_tasks;
} State;

void abort_mpi(const char *msg){
    fprintf(stderr, "[ERROR]: %s\n", msg);
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
}

int available_tasks(State *s){
    return s->task_list.count - s->task_list.next_index;
}

void check_pthread(int code, const char *msg){
    if (code != 0) abort_mpi(msg);
}

void compact_queue(State *s){
    TaskList *list = &s->task_list;
    int available = available_tasks(s);

    if (available <= 0) {
        list->count = 0;
        list->next_index = 0;
        return;
    }

    if (list->next_index > 0) {
        memmove(list->tasks, list->tasks + list->next_index, available * sizeof(Task));
        list->count = available;
        list->next_index = 0;
    }
}

int generate_task_weight(int rank, int size, int task_id, int iteration){
    int center = iteration % size;
    int distance = abs(rank - center);

    return BASE_WEIGHT + distance * DELTA_WEIGHT;
}

void init_tasks(State *s, int iter){
    TaskList *list = &s->task_list;

    pthread_mutex_lock(&list->mutex);

    list->count = 0;
    list->next_index = 0;

    int base = INITIAL_TASKS / s->size;
    int rem = INITIAL_TASKS % s->size;
    int local_cnt = base + (s->rank < rem ? 1 : 0);

    for (int i = 0; i < local_cnt; i++) {
        list->tasks[i].repeatNum = generate_task_weight(s->rank, s->size, i, iter);
        list->count++;
    }

    pthread_mutex_unlock(&list->mutex);
}

int pop_task(State *s, Task *t){
    TaskList *list = &s->task_list;

    if (available_tasks(s) <= 0) return 0;

    *t = list->tasks[list->next_index];
    list->next_index++;

    if (list->next_index == list->count) {
        list->count = 0;
        list->next_index = 0;
    }

    return 1;
}

int push_tasks(State *s, Task *tasks, int cnt){
    TaskList *list = &s->task_list;

    compact_queue(s);
    int pushed = 0;
    for (; pushed < cnt && list->count < MAX_TASKS; pushed++){
        list->tasks[list->count++] = tasks[pushed];
    }
    
    return pushed;
}

int take_tasks(State *s, Task *out){
    TaskList *list = &s->task_list;

    int available = available_tasks(s);
    if (available <= 1) return 0;

    int send_cnt = available / 2;
    if (send_cnt > MAX_SEND) send_cnt = MAX_SEND;

    for (int i = 0; i < send_cnt; i++) {
        out[i] = list->tasks[list->next_index++];
    }

    if (list->next_index == list->count) {
        list->count = 0;
        list->next_index = 0;
    }

    return send_cnt;
}

double execute(Task t){
    double res = 0.0;
    for (int i = 1; i <= t.repeatNum; i++) res += sqrt(i);
    return res;
}

void *server_thread(void *arg){
    State *s = (State*)arg;

    while (true) {
        int req;
        MPI_Status st;
        MPI_Recv(&req, 1, MPI_INT, MPI_ANY_SOURCE, TAG_REQUEST, MPI_COMM_WORLD, &st);

        if (req == REQ_STOP && st.MPI_SOURCE == s->rank) break;

        if (req == REQ_WORK) {
            Task to_send[MAX_SEND];

            pthread_mutex_lock(&s->task_list.mutex);

            int cnt = take_tasks(s, to_send);
            s->sent_tasks += cnt;

            pthread_mutex_unlock(&s->task_list.mutex);

            MPI_Send(&cnt, 1, MPI_INT, st.MPI_SOURCE, TAG_COUNT, MPI_COMM_WORLD);

            if (cnt > 0) {
                MPI_Send(to_send, cnt * sizeof(Task), MPI_BYTE, st.MPI_SOURCE, TAG_DATA, MPI_COMM_WORLD);
            }
        }
    }

    return NULL;
}

int request_work(State *s, int target){
    int req = REQ_WORK, cnt = 0;

    MPI_Send(&req, 1, MPI_INT, target, TAG_REQUEST, MPI_COMM_WORLD);
    MPI_Recv(&cnt, 1, MPI_INT, target, TAG_COUNT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    if (cnt <= 0) return 0;

    Task recv[MAX_SEND];
    MPI_Recv(recv, cnt * (int)sizeof(Task), MPI_BYTE, target, TAG_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    pthread_mutex_lock(&s->task_list.mutex);
    int pushed = push_tasks(s, recv, cnt);
    s->received_tasks += pushed;
    if (pushed != cnt){
        fprintf(stderr, "[ERROR]: Cannot push all tasks, pushed: %d/%d", pushed, cnt);
    }

    pthread_cond_signal(&s->task_list.work_ready_cond);
    pthread_mutex_unlock(&s->task_list.mutex);

    return cnt;
}

void *requester_thread(void *arg){
    State *s = (State*)arg;

    while (true) {
        pthread_mutex_lock(&s->task_list.mutex);

        while (!s->need_work && !s->requester_should_stop) {
            pthread_cond_wait(&s->task_list.need_work_cond, &s->task_list.mutex);
        }

        if (s->requester_should_stop) {
            pthread_mutex_unlock(&s->task_list.mutex);
            break;
        }

        s->need_work = 0;
        s->request_in_progress = 1;
        pthread_mutex_unlock(&s->task_list.mutex);

        if (s->size > 1) {
            for (int step = 1; step < s->size; step++) {
                int target = (s->rank + step) % s->size;
                if (request_work(s, target) > 0) break;
            }
        }

        pthread_mutex_lock(&s->task_list.mutex);
        s->request_in_progress = 0;
        pthread_cond_signal(&s->task_list.work_ready_cond);
        pthread_mutex_unlock(&s->task_list.mutex);
    }

    return NULL;
}

void *worker_thread(void *arg){
    State *s = (State*)arg;

    while (true) {
        Task t;

        pthread_mutex_lock(&s->task_list.mutex);

        while (available_tasks(s) == 0) {
            if (!s->need_work && !s->request_in_progress) {
                s->need_work = 1;
                pthread_cond_signal(&s->task_list.need_work_cond); 
            }

            while (available_tasks(s) == 0 && (s->need_work || s->request_in_progress)) {
                pthread_cond_wait(&s->task_list.work_ready_cond, &s->task_list.mutex);
            }

            if (available_tasks(s) == 0 && !s->need_work && !s->request_in_progress) {
                s->requester_should_stop = 1;
                pthread_cond_signal(&s->task_list.need_work_cond);
                pthread_mutex_unlock(&s->task_list.mutex);
                return NULL;
            }
        }

        int taken_flag = pop_task(s, &t);
        pthread_mutex_unlock(&s->task_list.mutex);

        if (taken_flag) {
            s->local_result += execute(t);
            s->processed_tasks++;
        }
    }
}


void reset_state(State *s){
    s->need_work = 0;
    s->request_in_progress = 0;
    s->requester_should_stop = 0;
    s->local_result = 0.0;
    s->processed_tasks = 0;
    s->received_tasks = 0;
    s->sent_tasks = 0;
}

void init_state(State *s, int rank, int size)
{
    memset(s, 0, sizeof(*s));
    s->rank = rank;
    s->size = size;

    check_pthread(pthread_mutex_init(&s->task_list.mutex, NULL), "mutex init");
    check_pthread(pthread_cond_init(&s->task_list.need_work_cond, NULL), "need_work_cond init");
    check_pthread(pthread_cond_init(&s->task_list.work_ready_cond, NULL), "work_ready_cond init");
}

void destroy_state(State *s){
    pthread_cond_destroy(&s->task_list.work_ready_cond);
    pthread_cond_destroy(&s->task_list.need_work_cond);
    pthread_mutex_destroy(&s->task_list.mutex);
}




int main(int argc, char **argv){
    int provided; 
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        abort_mpi("MPI_THREAD_MULTIPLE required");
    }

    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    State s;
    init_state(&s, rank, size);

    for (int iter = 0; iter < MAX_ITERATIONS; iter++) {
        MPI_Barrier(MPI_COMM_WORLD);

        reset_state(&s);
        init_tasks(&s, iter);

        pthread_t server, requester, worker;
        MPI_Barrier(MPI_COMM_WORLD);
        double start = MPI_Wtime();

        check_pthread(pthread_create(&server, NULL, server_thread, &s), "server thread create");
        check_pthread(pthread_create(&requester, NULL, requester_thread, &s), "requester thread create");
        check_pthread(pthread_create(&worker, NULL, worker_thread, &s), "worker thread create");

        check_pthread(pthread_join(worker, NULL), "join worker");
        check_pthread(pthread_join(requester, NULL), "join requester");

        MPI_Barrier(MPI_COMM_WORLD);

        int stop = REQ_STOP;
        MPI_Send(&stop, 1, MPI_INT, rank, TAG_REQUEST, MPI_COMM_WORLD);
        check_pthread(pthread_join(server, NULL), "join server");

        MPI_Barrier(MPI_COMM_WORLD);
        double time = MPI_Wtime() - start;

        double total_res, max_time;
        long total_processed, total_received, total_sent;

        MPI_Reduce(&s.local_result, &total_res, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&s.processed_tasks, &total_processed, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&s.received_tasks, &total_received, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&s.sent_tasks, &total_sent, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

        if (rank == iter){
            printf("* rank %d [ITER]=%d: processed=%ld received=%ld sent=%ld time=%.3f ms\n",
               rank, iter + 1, s.processed_tasks, s.received_tasks, s.sent_tasks, time * 1000);            
        }else{
            printf("rank %d [%d]: processed=%ld received=%ld sent=%ld time=%.3f ms\n",
               rank, iter + 1, s.processed_tasks, s.received_tasks, s.sent_tasks, time * 1000);
        }

        fflush(stdout);

        MPI_Barrier(MPI_COMM_WORLD);

        if (rank == 0) {
            printf("== TOTAL iter %d: result=%.2f processed=%ld received=%ld sent=%ld time=%.3f sec\n\n",
                   iter + 1, total_res, total_processed, total_received, total_sent, max_time);
        }
    }

    destroy_state(&s);
    MPI_Finalize();

    return 0;
}