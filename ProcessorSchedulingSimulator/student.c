/*
 * student.c
 * Multithreaded OS Simulation for CS 2200, Project 4
 *
 * This file contains the CPU scheduler for the simulation.  
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "os-sim.h"
#include "process.h"


/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 */
static pcb_t **current;
static pthread_mutex_t current_mutex;

typedef struct ll_node
{
	struct ll_node* next;
	struct ll_node* previous;
	pcb_t*          value;
} ll_node_t;

typedef struct ll_list
{
	int        size;
	ll_node_t* head;
	ll_node_t* tail;
} ll_list_t;

static ll_list_t* readyQueue = NULL;
static pthread_mutex_t readyQueue_mutexLock;
static pthread_cond_t readyQueue_cond_notEmpty;

static int mode;
static int roundRobin_quantum;

#define FIFO 0
#define ROUND_ROBIN 1
#define STATIC_PRIORITY 2

void initializeReadyQueue()
{
	pthread_mutex_init(&readyQueue_mutexLock, NULL);
	pthread_cond_init(&readyQueue_cond_notEmpty, NULL);

	readyQueue = (ll_list_t*)malloc(sizeof(ll_list_t));

	assert(NULL != readyQueue);

	readyQueue->head = NULL;
	readyQueue->tail = NULL;
	readyQueue->size = 0;
}

pcb_t* popHeadReadyQueue()
{
	ll_node_t* poppedHead;
	void* returnPointer;

	if (NULL == readyQueue->head)
		return NULL;
	else
	{
		--readyQueue->size;

		poppedHead = readyQueue->head;
		returnPointer = poppedHead->value;

		if (readyQueue->head == readyQueue->tail)
			readyQueue->tail = NULL;

		readyQueue->head = readyQueue->head->next;

		if (NULL != readyQueue->head)
			readyQueue->head->previous = NULL;

		/*printf("popped head address = %ld, returnPointer = %ld, returned PCB = {process_name = %s, process_id = %d}\n", (size_t)poppedHead, (size_t)returnPointer, ((pcb_t*)returnPointer)->name, ((pcb_t*)returnPointer)->pid);
		fflush(stdout);*/

		free(poppedHead);

		return returnPointer;
	}
}

pcb_t* popTailReadyQueue()
{
	ll_node_t* poppedTail;
	void* returnPointer;

	if (NULL == readyQueue->tail)
		return NULL;
	else
	{
		--readyQueue->size;

		poppedTail = readyQueue->tail;
		returnPointer = poppedTail->value;

		if (readyQueue->tail == readyQueue->head)
			readyQueue->head = NULL;

		readyQueue->tail = readyQueue->tail->previous;

		if (NULL != readyQueue->tail)
			readyQueue->tail->next = NULL;

		free(poppedTail);

		return returnPointer;
	}
}

void pushHeadReadyQueue(pcb_t* addedPCB)
{
	ll_node_t* newHead;

	newHead = (ll_node_t*)malloc(sizeof(ll_node_t));
	assert(NULL != newHead);

	if (NULL == readyQueue->head)
	{
		readyQueue->head = readyQueue->tail = newHead;
		newHead->next = newHead->previous = NULL;
	}
	else
	{
		newHead->previous = NULL;
		newHead->next = readyQueue->head;
		readyQueue->head->previous = newHead;

		readyQueue->head = newHead;
	}

	newHead->value = addedPCB;

	++readyQueue->size;
}

void pushTailReadyQueue(pcb_t* addedPCB)
{
	ll_node_t* newTail;

	newTail = (ll_node_t*)malloc(sizeof(ll_node_t));
	assert(NULL != newTail);

	if (NULL == readyQueue->tail)
	{
		readyQueue->head = readyQueue->tail = newTail;
		newTail->next = newTail->previous = NULL;
	}
	else
	{
		newTail->next = NULL;
		newTail->previous = readyQueue->tail;
		readyQueue->tail->next = newTail;

		readyQueue->tail = newTail;
	}

	newTail->value = addedPCB;

	++readyQueue->size;
}

void pushPriorityReadyQueue(pcb_t* addedPCB)
{
	ll_node_t* newNode;
	ll_node_t* currentNode;
	ll_node_t* tempNodeFront;
	ll_node_t* tempNodeBack;

	newNode = (ll_node_t*)malloc(sizeof(ll_node_t));
	assert(NULL != newNode);

	if (NULL == readyQueue->tail)
	{
		readyQueue->head = readyQueue->tail = newNode;
		newNode->next = newNode->previous = NULL;
		newNode->value = addedPCB;
	}
	else
	{
		newNode->value = addedPCB;
		newNode->next = NULL;
		newNode->previous = readyQueue->tail;

		readyQueue->tail->next = newNode;
		readyQueue->tail = newNode;

		currentNode = readyQueue->tail;

		while (currentNode->previous != NULL)
		{
			/*printf("Fixing the priority queue, currentPriority=%d, currentPreviousPriority=%d\n", currentNode->value->static_priority, currentNode->previous->value->static_priority);*/
			if (currentNode->value->static_priority > currentNode->previous->value->static_priority)
			{
				if (readyQueue->tail == currentNode)
					readyQueue->tail = currentNode->previous;

				if (readyQueue->head == currentNode->previous)
					readyQueue->head = currentNode;

				tempNodeFront = currentNode->previous;
				tempNodeBack = currentNode;

				tempNodeFront->next = tempNodeBack->next;
				tempNodeBack->previous = tempNodeFront->previous;

				if (NULL != tempNodeFront->previous)
					tempNodeFront->previous->next = tempNodeBack;
				if (NULL != tempNodeBack->next)
					tempNodeBack->next->previous = tempNodeFront;

				tempNodeFront->previous = tempNodeBack;
				tempNodeBack->next = tempNodeFront;
			}
			else
			{
				break;
			}
		}
	}

	++readyQueue->size;
}

static int cpu_count = 0;

/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which 
 *	you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Call context_switch(), to tell the simulator which process to execute
 *     next on the CPU.  If no process is runnable, call context_switch()
 *     with a pointer to NULL to select the idle process.
 *	The current array (see above) is how you access the currently running process indexed by the cpu id. 
 *	See above for full description.
 *	context_switch() is prototyped in os-sim.h. Look there for more information 
 *	about it and its parameters.
 */
static void schedule(unsigned int cpu_id)
{
	pcb_t* nextPCB;

	nextPCB = popHeadReadyQueue();

	if (NULL != nextPCB)
	{
		nextPCB->state = PROCESS_RUNNING;

		if (FIFO == mode || STATIC_PRIORITY == mode)
			context_switch(cpu_id, nextPCB, -1);
		else if (ROUND_ROBIN == mode)
			context_switch(cpu_id, nextPCB, roundRobin_quantum);
	}
	else
	{
		context_switch(cpu_id, nextPCB, -1);
	}

	pthread_mutex_lock(&current_mutex);
		current[cpu_id] = nextPCB;
	pthread_mutex_unlock(&current_mutex);
}


/*
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * This function should block until a process is added to your ready queue.
 * It should then call schedule() to select the process to run on the CPU.
 */
extern void idle(unsigned int cpu_id)
{
	pthread_mutex_lock(&readyQueue_mutexLock);

	while (readyQueue->size == 0)
	{
		pthread_cond_wait(&readyQueue_cond_notEmpty, &readyQueue_mutexLock);
	}

	schedule(cpu_id);

	pthread_mutex_unlock(&readyQueue_mutexLock);
}


/*
 * preempt() is the handler called by the simulator when a process is
 * preempted due to its timeslice expiring.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 */
extern void preempt(unsigned int cpu_id)
{
	pcb_t* preemptedPCB;

	pthread_mutex_lock(&current_mutex);
		preemptedPCB = current[cpu_id];
		preemptedPCB->state = PROCESS_READY;
	pthread_mutex_unlock(&current_mutex);

	pthread_mutex_lock(&readyQueue_mutexLock);

		if (STATIC_PRIORITY == mode)
		{
			pushPriorityReadyQueue(preemptedPCB);
		}
		else
		{
			pushTailReadyQueue(preemptedPCB);
		}

		/*pthread_cond_broadcast(&readyQueue_cond_notEmpty);*/

		while (readyQueue->size == 0)
		{
			printf(" > This should NEVER print, or else you added to the queue and it's still empty\n");
			pthread_cond_wait(&readyQueue_cond_notEmpty, &readyQueue_mutexLock);
		}

		schedule(cpu_id);

	pthread_mutex_unlock(&readyQueue_mutexLock);
}


/*
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * It should mark the process as WAITING, then call schedule() to select
 * a new process for the CPU.
 */
extern void yield(unsigned int cpu_id)
{
	pthread_mutex_lock(&current_mutex);

		current[cpu_id]->state = PROCESS_WAITING;

	pthread_mutex_unlock(&current_mutex);

	schedule(cpu_id);
}


/*
 * terminate() is the handler called by the simulator when a process completes.
 * It should mark the process as terminated, then call schedule() to select
 * a new process for the CPU.
 */
extern void terminate(unsigned int cpu_id)
{
	pthread_mutex_lock(&current_mutex);

		current[cpu_id]->state = PROCESS_TERMINATED;

	pthread_mutex_unlock(&current_mutex);

	schedule(cpu_id);
}


/*
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.  It should perform the following tasks:
 *
 *   1. Mark the process as READY, and insert it into the ready queue.
 *
 *   2. If the scheduling algorithm is static priority, wake_up() may need
 *	  to preempt the CPU with the lowest priority process to allow it to
 *	  execute the process which just woke up.  However, if any CPU is
 *	  currently running idle, or all of the CPUs are running processes
 *	  with a higher priority than the one which just woke up, wake_up()
 *	  should not preempt any CPUs.
 *	To preempt a process, use force_preempt(). Look in os-sim.h for 
 * 	its prototype and the parameters it takes in.
 */
extern void wake_up(pcb_t *process)
{
	int n;
	int cpu_id;
	int foundCPU;
	unsigned int minimumPriority;

	process->state = PROCESS_READY;

	if (STATIC_PRIORITY == mode)
	{
		pthread_mutex_lock(&readyQueue_mutexLock);

			pushPriorityReadyQueue(process);
			pthread_cond_broadcast(&readyQueue_cond_notEmpty);

		pthread_mutex_unlock(&readyQueue_mutexLock);

		foundCPU = 0;
		minimumPriority = 11;

		pthread_mutex_lock(&current_mutex);

		for (cpu_id = 0; cpu_id < cpu_count; ++cpu_id)
			if (NULL == current[cpu_id])
			{
				foundCPU = 1;
				break;
			}

		if (!foundCPU)
		{
			for (n = 0; n < cpu_count; ++n)
			{
				if (current[n]->static_priority < minimumPriority && current[n]->static_priority < process->static_priority)
				{
					minimumPriority = current[n]->static_priority;
					cpu_id = n;
				}
			}

			if (minimumPriority <= 10)
			{
				current[cpu_id]->state = PROCESS_READY;

				pthread_mutex_unlock(&current_mutex);

				/*printf("  -- Preempting CPU[%d],  current[%d]->static_priority = %d, process->static_priority = %d\n", cpu_id, cpu_id, current[cpu_id]->static_priority, process->static_priority);*/
				force_preempt(cpu_id);

				pthread_mutex_lock(&current_mutex);
			}
			/*else
			{
				printf("  -- The currently running processes have higher priority than %s\n", process->name);
			}*/
		}

		pthread_mutex_unlock(&current_mutex);
	}
	else
	{
		pthread_mutex_lock(&readyQueue_mutexLock);

			pushTailReadyQueue(process);
			pthread_cond_broadcast(&readyQueue_cond_notEmpty);

		pthread_mutex_unlock(&readyQueue_mutexLock);
	}
}


/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -r and -p command-line parameters.
 */
int main(int argc, char *argv[])
{
	int n;

	if (argc == 2)
	{
		mode = FIFO;
	}
	else if (argc == 3)
	{
		if (argv[2][0] == '-' && argv[2][1] == 'p' && argv[2][2] == '\0')
		{
			mode = STATIC_PRIORITY;
		}
		else
		{
			fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
				"Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
				"    Default : FIFO Scheduler\n"
				"         -r : Round-Robin Scheduler\n"
				"         -p : Static Priority Scheduler\n\n");
			return -1;
		}
	}
	else if (argc == 4)
	{
		if (argv[2][0] == '-' && argv[2][1] == 'r' && argv[2][2] == '\0')
		{
			mode = ROUND_ROBIN;
			roundRobin_quantum = atoi(argv[3]);
			printf("%d\n", roundRobin_quantum);
		}
		else
		{
			fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
				"Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
				"    Default : FIFO Scheduler\n"
				"         -r : Round-Robin Scheduler\n"
				"         -p : Static Priority Scheduler\n\n");
			return -1;
		}
	}
	else
	{
		fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
			"Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
			"    Default : FIFO Scheduler\n"
			"         -r : Round-Robin Scheduler\n"
			"         -p : Static Priority Scheduler\n\n");
		return -1;
	}
	cpu_count = atoi(argv[1]);
	assert(cpu_count >= 1 && cpu_count <= 16);

	/* Allocate the current[] array and its mutex */
	current = malloc(sizeof(pcb_t*) * cpu_count);
	assert(current != NULL);

	pthread_mutex_init(&current_mutex, NULL);

	for (n = 0; n < cpu_count; ++n)
		current[n] = NULL;

	initializeReadyQueue();

	/* Start the simulator in the library */
	start_simulator(cpu_count);

	return 0;
}