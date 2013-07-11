// os345p2.c - Multi-tasking
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the CS345 projects.          **
// ** It comes "as is" and "unwarranted."  As such, when you use part   **
// ** or all of the code, it becomes "yours" and you are responsible to **
// ** understand any algorithm or method presented.  Likewise, any      **
// ** errors or problems become your responsibility to fix.             **
// **                                                                   **
// ** NOTES:                                                            **
// ** -Comments beginning with "// ??" may require some implementation. **
// ** -Tab stops are set at every 3 spaces.                             **
// ** -The function API's in "OS345.h" should not be altered.           **
// **                                                                   **
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// ***********************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include <time.h>
#include "os345.h"

#define my_printf	printf

// ***********************************************************************
// project 2 variables
static Semaphore* sTask10;					// task 1 semaphore
static Semaphore* sTask11;					// task 2 semaphore

extern Semaphore* tics10sec;
extern int* blockedQueue;
extern int** readyQueue;

extern TCB tcb[];								// task control block
extern int curTask;							// current task #
extern Semaphore* semaphoreList;			// linked list of active semaphores
extern jmp_buf reset_context;				// context of kernel stack

// ***********************************************************************
// project 2 functions and tasks

int signalTask(int, char**);
int ImAliveTask(int, char**);

// ***********************************************************************
// ***********************************************************************
// project2 command
int P2_project2(int argc, char* argv[])
{
	static char* s1Argv[] = {"sTask1", "sTask10"};
	static char* s2Argv[] = {"sTask2", "sTask11"};
	static char* aliveArgv[] = {"ImAlive", "3"};

	printf("\nStarting Project 2");
	SWAP;

	char *argvTimer[] = { "timeThis"};
	int i;
	for (i=0 ; i < 10 ; i++){
		createTask("timeThis", P2_timeThis, HIGH_PRIORITY, 1, argvTimer);
	}
	// start tasks looking for sTask semaphores
	createTask("sTask1",				// task name
			signalTask,				// task
			VERY_HIGH_PRIORITY,	// task priority
			2,							// task argc
			s1Argv);					// task argument pointers

	createTask("sTask2",				// task name
			signalTask,				// task
			VERY_HIGH_PRIORITY,	// task priority
			2,							// task argc
			s2Argv);					// task argument pointers

	createTask("ImAlive",				// task name
			ImAliveTask,			// task
			LOW_PRIORITY,			// task priority
			2,							// task argc
			aliveArgv);				// task argument pointers

	createTask("ImAlive",				// task name
			ImAliveTask,			// task
			LOW_PRIORITY,			// task priority
			2,							// task argc
			aliveArgv);				// task argument pointers

	return 0;
} // end P2_project2

int P2_timeThis(int argc, char* argv[]) {
	while (1) {
		SWAP
		SEM_WAIT(tics10sec);
		SWAP
		time_t rawtime;
		struct tm * timeinfo;
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		printf("\nThe current time is %sThe current task is %d.\n",asctime(timeinfo),curTask);
		SWAP
		SEM_WAIT(tics10sec);
		SWAP
	}
	return 0;
}

// ***********************************************************************
// ***********************************************************************
// list tasks command
int P2_listTasks(int argc, char* argv[])
{
	int i;

	//	?? 1) List all tasks in all queues
	// ?? 2) Show the task stake (new, running, blocked, ready)
	// ?? 3) If blocked, indicate which semaphore
	/*
	for (i=0; i<MAX_TASKS; i++)
	{
		if (tcb[i].name)
		{
			printf("\n%4d/%-4d%20s%4d  ", i, tcb[i].parent,
					tcb[i].name, tcb[i].priority);
			if (tcb[i].signal & mySIGSTOP) my_printf("Paused");
			else if (tcb[i].state == S_NEW) my_printf("New");
			else if (tcb[i].state == S_READY) my_printf("Ready");
			else if (tcb[i].state == S_RUNNING) my_printf("Running");
			else if (tcb[i].state == S_BLOCKED) my_printf("Blocked    %s",
					tcb[i].event->name);
			else if (tcb[i].state == S_EXIT) my_printf("Exiting");
			swapTask();
		}
	}
	 */
	int pri,tsk;
	for ( pri = MAX_PRIORITY ; pri >= 0 ; pri--) { // start at highest priority
		for ( tsk = 0 ; tsk <= MAX_TASKS ; tsk++) { // do FIFO
			if(readyQueue[pri][tsk] != -1) {
				i = readyQueue[pri][tsk];
				printf("\n%4d/%-4d%20s%4d  ", i, tcb[i].parent,
						tcb[i].name, tcb[i].priority);
				if (tcb[i].signal & mySIGSTOP) my_printf("Paused");
				else if (tcb[i].state == S_NEW) my_printf("New");
				else if (tcb[i].state == S_READY) my_printf("Ready");
				else if (tcb[i].state == S_RUNNING) my_printf("Running");
				else if (tcb[i].state == S_BLOCKED) my_printf("Blocked    %s",
						tcb[i].event->name);
				else if (tcb[i].state == S_EXIT) my_printf("Exiting");
				swapTask();
			}
		}
	}
	for ( tsk=0; tsk <=MAX_TASKS ; tsk++) {
		if(blockedQueue[tsk] != -1) {
			i = blockedQueue[tsk];
			printf("\n%4d/%-4d%20s%4d  ", i, tcb[i].parent,
					tcb[i].name, tcb[i].priority);
			if (tcb[i].signal & mySIGSTOP) my_printf("Paused");
			else if (tcb[i].state == S_NEW) my_printf("New");
			else if (tcb[i].state == S_READY) my_printf("Ready");
			else if (tcb[i].state == S_RUNNING) my_printf("Running");
			else if (tcb[i].state == S_BLOCKED) my_printf("Blocked    %s",
					tcb[i].event->name);
			else if (tcb[i].state == S_EXIT) my_printf("Exiting");
			swapTask();
		}
	}

	return 0;
} // end P2_listTasks



// ***********************************************************************
// ***********************************************************************
// list semaphores command
//
int match(char* mask, char* name)
{
	int i,j;

	// look thru name
	i = j = 0;
	if (!mask[0]) return 1;
	while (mask[i] && name[j])
	{
		if (mask[i] == '*') return 1;
		if (mask[i] == '?') ;
		else if ((mask[i] != toupper(name[j])) && (mask[i] != tolower(name[j]))) return 0;
		i++;
		j++;
	}
	if (mask[i] == name[j]) return 1;
	return 0;
} // end match

int P2_listSems(int argc, char* argv[])				// listSemaphores
{
	Semaphore* sem = semaphoreList;
	while(sem)
	{
		if ((argc == 1) || match(argv[1], sem->name))
		{
			printf("\n%20s  %c  %d  %s", sem->name, (sem->type?'C':'B'), sem->state,
					tcb[sem->taskNum].name);
		}
		sem = (Semaphore*)sem->semLink;
	}
	return 0;
} // end P2_listSems



// ***********************************************************************
// ***********************************************************************
// reset system
int P2_reset(int argc, char* argv[])						// reset
{
	longjmp(reset_context, POWER_DOWN_RESTART);
	// not necessary as longjmp doesn't return
	return 0;

} // end P2_reset



// ***********************************************************************
// ***********************************************************************
// kill task

int P2_killTask(int argc, char* argv[])			// kill task
{
	int taskId = INTEGER(argv[1]);					// convert argument 1

	if ((taskId > 0) && tcb[taskId].name)			// check for single task
	{
		my_printf("\nKill Task %d", taskId);
		tcb[taskId].state = S_EXIT;
		return 0;
	}
	else if (taskId < 0)									// check for all tasks
	{
		printf("\nKill All Tasks");
		for (taskId=1; taskId<MAX_TASKS; taskId++)
		{
			if (tcb[taskId].name)
			{
				my_printf("\nKill Task %d", taskId);
				tcb[taskId].state = S_EXIT;
			}
		}
	}
	else														// invalid argument
	{
		my_printf("\nIllegal argument or Invalid Task");
	}
	return 0;
} // end P2_killTask



// ***********************************************************************
// ***********************************************************************
// signal command
void sem_signal(Semaphore* sem)		// signal
{
	if (sem)
	{
		printf("\nSignal %s", sem->name);
		SEM_SIGNAL(sem);
	}
	else my_printf("\nSemaphore not defined!");
	return;
} // end sem_signal



// ***********************************************************************
int P2_signal1(int argc, char* argv[])		// signal1
{
	SEM_SIGNAL(sTask10);
	return 0;
} // end signal

int P2_signal2(int argc, char* argv[])		// signal2
{
	SEM_SIGNAL(sTask11);
	return 0;
} // end signal



// ***********************************************************************
// ***********************************************************************
// signal task
//
#define COUNT_MAX	5
//
int signalTask(int argc, char* argv[])
{
	int count = 0;					// task variable

	// create a semaphore
	Semaphore** mySem = (!strcmp(argv[1], "sTask10")) ? &sTask10 : &sTask11;
	*mySem = createSemaphore(argv[1], 0, 0);

	// loop waiting for semaphore to be signaled
	while(count < COUNT_MAX)
	{
		SEM_WAIT(*mySem);			// wait for signal
		printf("\n%s  Task[%d], count=%d", tcb[curTask].name, curTask, ++count);
	}
	return 0;						// terminate task
} // end signalTask



// ***********************************************************************
// ***********************************************************************
// I'm alive task
int ImAliveTask(int argc, char* argv[])
{
	int i;							// local task variable
	while (1)
	{
		printf("\n(%d) I'm Alive!\n", curTask);
		for (i=0; i<100000; i++) swapTask();
	}
	return 0;						// terminate task
} // end ImAliveTask
