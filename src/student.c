/*
 * student.c
 * Multithreaded OS Simulation for CS 2200
 *
 * This file contains the CPU scheduler for the simulation.
 */
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "student.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/** Function prototypes **/
extern void idle(unsigned int cpu_id);
extern void preempt(unsigned int cpu_id);
extern void yield(unsigned int cpu_id);
extern void terminate(unsigned int cpu_id);
extern void wake_up(pcb_t *process);

/**
 * current is an array of pointers to the currently running processes,
 * each pointer corresponding to each CPU in the simulation.
 */
static pcb_t **current;
/* rq is a pointer to a struct you should use for your ready queue implementation.*/
static queue_t *rq;

/**
 * current and rq are accessed by multiple threads, so you will need to use
 * a mutex to protect it (ready queue).
 *
 * The condition variable queue_not_empty has been provided for you
 * to use in conditional waits and signals.
 */
static pthread_mutex_t current_mutex;
static pthread_mutex_t queue_mutex;
static pthread_cond_t queue_not_empty;

/* keeps track of the scheduling alorightem and cpu count */
static sched_algorithm_t scheduler_algorithm;
static unsigned int cpu_count;
static int rr_time_slice = -1;
/** ------------------------Problem 0 & 2-----------------------------------
 * Checkout PDF Section 2 and 4 for this problem
 *
 * enqueue() is a helper function to add a process to the ready queue.
 *
 * @param queue pointer to the ready queue
 * @param process process that we need to put in the ready queue
 */
void enqueue(queue_t *queue, pcb_t *process)
{
    /* FIX ME */
    // case 1: using preemptive priority scheduling
    if (scheduler_algorithm == PR)
    {
        // if the queue is empty
        if (is_empty(queue))
        {
            queue->head = process;
            queue->tail = process;
            process->next = NULL;
        }
        // if the queue is not empty
        else
        {
            // finding the right place to add the current process
            pcb_t *curr = queue->head;
            pcb_t *prev = NULL;

            while (curr != NULL && process->priority >= curr->priority)
            {
                prev = curr;
                curr = curr->next;
            }

            // process has highest priority
            if (!prev)
            {
                process->next = queue->head;
                queue->head = process;
            }

            // process has lowest priority
            else if (!curr)
            {
                queue->tail->next = process;
                queue->tail = process;
                process->next = NULL;
            }
            // process is in between
            else
            {
                process->next = curr;
                prev->next = process;
            }
        }
    }
    else // case 2: non-priority scheduler
    {
        // if the queue is empty
        if (is_empty(queue))
        {
            queue->head = process;
            queue->tail = process;
            process->next = NULL;
        }
        // if the queue is not empty
        else
        {
            queue->tail->next = process;
            queue->tail = process;
            process->next = NULL;
        }
    }
}

/**
 * dequeue() is a helper function to remove a process to the ready queue.
 *
 * @param queue pointer to the ready queue
 */
pcb_t *dequeue(queue_t *queue)
{
    /* FIX ME */
    // if the queue is not empty
    if (!is_empty(queue))
    {
        pcb_t *temp = queue->head;
        queue->head = queue->head->next;
        return temp;
    }
    // if the queue is empty
    return NULL;
}

/** ------------------------Problem 0-----------------------------------
 * Checkout PDF Section 2 for this problem
 *
 * is_empty() is a helper function that returns whether the ready queue
 * has any processes in it.
 *
 * @param queue pointer to the ready queue
 *
 * @return a boolean value that indicates whether the queue is empty or not
 */
bool is_empty(queue_t *queue)
{
    /* FIX ME */
    if (queue->head)
        return false;
    return true;
}

/** ------------------------Problem 1B & 3-----------------------------------
 * Checkout PDF Section 3 and 5 for this problem
 *
 * schedule() is your CPU scheduler.
 *
 * Remember to specify the timeslice if the scheduling algorithm is Round-Robin
 *
 * @param cpu_id the target cpu we decide to put our process in
 */
static void schedule(unsigned int cpu_id)
{
    /* FIX ME */
    // locking the current
    pthread_mutex_lock(&current_mutex);
    // locking the queue
    pthread_mutex_lock(&queue_mutex);

    // initialize process pointer
    pcb_t *process = (pcb_t *)NULL;

    // if the ready queue is not empty
    if (!is_empty(rq))
    {
        // dequeue the process from the ready queue
        process = dequeue(rq);

        // make sure process is not NULL
        assert(process);
        // set the state of the process to running
        process->state = PROCESS_RUNNING;
        // put the process in the current
        current[cpu_id] = process;
    }
    // unlocking the queue
    pthread_mutex_unlock(&queue_mutex);
    // unlocking the current
    pthread_mutex_unlock(&current_mutex);
    context_switch(cpu_id, process, rr_time_slice);
}

/**  ------------------------Problem 1A-----------------------------------
 * Checkout PDF Section 3 for this problem
 *
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * @param cpu_id the cpu that is waiting for process to come in
 */
extern void idle(unsigned int cpu_id)
{
    /* FIX ME */
    // locking the queue
    pthread_mutex_lock(&queue_mutex);
    // waiting on the queue_not_empty signal
    while (is_empty(rq))
        pthread_cond_wait(&queue_not_empty, &queue_mutex);
    // unlocking the queue
    pthread_mutex_unlock(&queue_mutex);

    // calling the scheduler
    schedule(cpu_id);
    /*
     * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
     *
     * idle() must block when the ready queue is empty, or else the CPU threads
     * will spin in a loop.  Until a ready queue is implemented, we'll put the
     * thread to sleep to keep it from consuming 100% of the CPU time.  Once
     * you implement a proper idle() function using a condition variable,
     * remove the call to mt_safe_usleep() below.
     */
    // mt_safe_usleep(1000000);
}

/** ------------------------Problem 2 & 3-----------------------------------
 * Checkout Section 4 and 5 for this problem
 *
 * preempt() is the handler used in Round-robin and Preemptive Priority
 * Scheduling
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 *
 * @param cpu_id the cpu in which we want to preempt process
 */
extern void preempt(unsigned int cpu_id)
{
    /* FIX ME */
    // locking the current mutex
    pthread_mutex_lock(&current_mutex);

    // check if the current cpu_id is valid
    assert(cpu_id < cpu_count);
    // check if the current process is not null
    assert(current[cpu_id] != NULL);

    // setting the current process to ready
    current[cpu_id]->state = PROCESS_READY;

    //// putting current process to the end of ready queue
    // locking the queue mutex
    pthread_mutex_lock(&queue_mutex);
    enqueue(rq, current[cpu_id]);

    // unlocking the queue mutex
    pthread_mutex_unlock(&queue_mutex);
    // clear the current process
    current[cpu_id] = NULL;
    // unlocking the current mutex
    pthread_mutex_unlock(&current_mutex);

    // calling schedule to run the next process
    schedule(cpu_id);
}

/**  ------------------------Problem 1-----------------------------------
 * Checkout PDF Section 3 for this problem
 *
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * @param cpu_id the cpu that is yielded by the process
 */
extern void yield(unsigned int cpu_id)
{
    /* FIX ME */
    // locking the current mutex
    pthread_mutex_lock(&current_mutex);

    // check if the current cpu_id is valid
    assert(cpu_id < cpu_count);
    // check if the current process is not null
    assert(current[cpu_id] != NULL);

    // setting the current process to waiting
    current[cpu_id]->state = PROCESS_WAITING;
    // clear the current process
    current[cpu_id] = NULL;
    // unlocking the current mutex
    pthread_mutex_unlock(&current_mutex);

    // calling schedule to run the next process
    schedule(cpu_id);
}

/**  ------------------------Problem 1-----------------------------------
 * Checkout PDF Section 3
 *
 * terminate() is the handler called by the simulator when a process completes.
 *
 * @param cpu_id the cpu we want to terminate
 */
extern void terminate(unsigned int cpu_id)
{
    /* FIX ME */
    // locking the current mutex
    pthread_mutex_lock(&current_mutex);

    // check if the current cpu_id is valid
    assert(cpu_id < cpu_count);
    // check if the current process is not null
    assert(current[cpu_id] != NULL);

    // setting the current process to terminated
    current[cpu_id]->state = PROCESS_TERMINATED;
    // clear the current process
    current[cpu_id] = NULL;
    // unlocking the current mutex
    pthread_mutex_unlock(&current_mutex);
    // calling schedule to run the next process
    schedule(cpu_id);
}

/**  ------------------------Problem 1A & 3---------------------------------
 * Checkout PDF Section 3 and 4 for this problem
 *
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes. This method will also need to handle priority,
 * Look in section 5 of the PDF for more info.
 *
 * @param process the process that finishes I/O and is ready to run on CPU
 */
extern void wake_up(pcb_t *process)
{
    /* FIX ME */
    // case 1: using preemptive priority scheduling
    if (scheduler_algorithm == PR)
    {
        // locking the queue mutex
        pthread_mutex_lock(&queue_mutex);

        // check if the process is not null
        assert(process != NULL);

        // setting the process to ready
        process->state = PROCESS_READY;
        // putting the process back to the ready queue
        enqueue(rq, process);

        // sending queue_not_empty signal
        pthread_cond_signal(&queue_not_empty);

        // unlocking the queue mutex
        pthread_mutex_unlock(&queue_mutex);

        // locking the current mutex
        pthread_mutex_lock(&current_mutex);
        // case 1.1: there is an idle CPU
        for (unsigned int i = 0; i < cpu_count; i++)
        {
            if (!current[i])
            {
                // unlocking the current mutex
                pthread_mutex_unlock(&current_mutex);
                return;
            }
        }
        // now it is guaranteed that there are not idle CPU
        // finding the CPU process that has the lowest priority
        unsigned int lowest_priority_CPU_id = 0;
        for (unsigned int i = 0; i < cpu_count; i++)
        {
            if (current[i]->priority > current[lowest_priority_CPU_id]->priority)
                lowest_priority_CPU_id = i;
        }

        // case 1.2.1: the lowest-priority running process has higher priority than the current process
        if (current[lowest_priority_CPU_id]->priority < process->priority)
        { // unlocking the current mutex
            pthread_mutex_unlock(&current_mutex);
            return;
        }


        // unlocking the current mutex
        pthread_mutex_unlock(&current_mutex);

        // case 1.2.2: doing preemption
        force_preempt(lowest_priority_CPU_id);
    }
    else // case 2: non-preemptive scheduler
    {
        // locking the queue mutex
        pthread_mutex_lock(&queue_mutex);

        // check if the process is not null
        assert(process != NULL);

        // setting the process to ready
        process->state = PROCESS_READY;
        // putting the process back to the ready queue
        enqueue(rq, process);

        // sending queue_not_empty signal
        pthread_cond_signal(&queue_not_empty);

        // unlocking the queue mutex
        pthread_mutex_unlock(&queue_mutex);
    }
}

/**
 * main() simply parses command line arguments, then calls start_simulator().
 * Add support for -r and -p parameters. If no argument has been supplied,
 * you should default to FCFS.
 *
 * HINT:
 * Use the scheduler_algorithm variable (see student.h) in your scheduler to
 * keep track of the scheduling algorithm you're using.
 */
int main(int argc, char *argv[])
{

    /*
     * FIX ME
     */

    if (argc == 4 && strcmp(argv[2], "-r") == 0)
    {
        scheduler_algorithm = RR;
        rr_time_slice = atoi(argv[3]);
    }
    else if (argc == 3 && strcmp(argv[2], "-p") == 0)
        scheduler_algorithm = PR;
    else if (argc == 2)
        scheduler_algorithm = FCFS;
    else
    {
        fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
                        "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
                        "    Default : FCFS Scheduler\n"
                        "         -r : Round-Robin Scheduler\n1\n"
                        "         -p : Priority Scheduler\n");
        return -1;
    }
    /* Parse the command line arguments */
    cpu_count = strtoul(argv[1], NULL, 0);

    /* Allocate the current[] array and its mutex */
    current = malloc(sizeof(pcb_t *) * cpu_count);
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_cond_init(&queue_not_empty, NULL);
    rq = (queue_t *)malloc(sizeof(queue_t));
    assert(rq != NULL);

    /* Start the simulator in the library */
    start_simulator(cpu_count);

    return 0;
}

#pragma GCC diagnostic pop
