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
#include "os345.h"

#define my_printf       printf

// ***********************************************************************
// project 2 variables
static Semaphore* s1Sem;                                        // task 1 semaphore
static Semaphore* s2Sem;                                        // task 2 semaphore

extern TCB tcb[];                                                            // task control block
extern int curTask;                                                     // current task #
extern Semaphore* semaphoreList;                        // linked list of active semaphores
extern jmp_buf reset_context;                           // context of kernel stack
extern int rq[MAX_TASKS];
extern int taskCount;
extern Semaphore* tics10sec;
// ***********************************************************************
// project 2 functions and tasks

int signalTask(int, char**);
int ImAliveTask(int, char**);

// ***********************************************************************
// ***********************************************************************
// project2 command
int P2_project2(int argc, char* argv[]) {
	static char* s1Argv[] = { "signal1", "s1Sem" };
	static char* s2Argv[] = { "signal2", "s2Sem" };
	static char* aliveArgv[] = { "I'm Alive", "3" };
	static char* ten[] = { "ten" };
	//s1Sem=createSemaphore("s1Sem", BINARY, 0);
	//s2Sem=createSemaphore("s2Sem", BINARY, 0);
	printf("\nStarting Project 2");
	SWAP
	;
	//printf("11");
	// start tasks looking for sTask semaphores
	createTask("signal1",                           // task name
			signalTask,                             // task
			VERY_HIGH_PRIORITY,     // task priority
			2,                                                      // task argc
			s1Argv);                                        // task argument pointers
//      printf("!!");
	createTask("signal2",                           // task name
			signalTask,                             // task
			VERY_HIGH_PRIORITY,     // task priority
			2,                                                      // task argc
			s2Argv);                                        // task argument pointers

	createTask("I'm Alive",                         // task name
			ImAliveTask,                    // task
			LOW_PRIORITY,                   // task priority
			2,                                                      // task argc
			aliveArgv);                             // task argument pointers

	createTask("I'm Alive",                         // task name
			ImAliveTask,                    // task
			LOW_PRIORITY,                   // task priority
			2,                                                      // task argc
			aliveArgv);                             // task argument pointers
	int i;
	for (i = 0; i < 10; i++) {       //printf("\nCreating No. %d tenSecond task",i);
		createTask("tenSecond", P2_tenSecond, HIGH_PRIORITY, 1, ten);
	}
	return 0;
} // end P2_project2

// ***********************************************************************
// ***********************************************************************
// list tasks command
int P2_listTasks(int argc, char* argv[]) {
	int i;

//      ?? 1) List all tasks in all queues
// ?? 2) Show the task stake (new, running, blocked, ready)
// ?? 3) If blocked, indicate which semaphore
	printf("\nReadyQueue  taskCount:%d : currentTask:%d", taskCount, curTask);
	/*for (i=0; i<taskCount; i++)
	 {
	 int tid=rq[i];
	 if(tid==-1)
	 {
	 break;
	 }
	 if (tcb[tid].name)
	 {
	 printf("\n%4d/%-4d%20s%4d  ", tid, tcb[tid].parent,
	 tcb[tid].name, tcb[tid].priority);
	 if (tcb[tid].signal & mySIGSTOP) my_printf("Paused");
	 else if (tcb[tid].state == S_NEW) my_printf("New");
	 else if (tcb[tid].state == S_READY) my_printf("Ready");
	 else if (tcb[tid].state == S_RUNNING) my_printf("Running");
	 else if (tcb[tid].state == S_BLOCKED) my_printf("Blocked    %s",
	 tcb[tid].event->name);
	 else if (tcb[tid].state == S_EXIT) my_printf("Exiting");
	 //swapTask();
	 }
	 }*/

	for (i = 0; i < taskCount; i++) {
		int tid = rq[i];
		if (tid == -1) {
			break;
		}
		if (tcb[tid].name) {
			printf("\n%4d/%-4d%20s%4d  ", tid, tcb[tid].parent, tcb[tid].name, tcb[tid].priority);
			if (tcb[tid].signal & mySIGSTOP)
				my_printf("Paused");
			else if (tcb[tid].state == S_NEW)
				my_printf("New");
			else if (tcb[tid].state == S_READY)
				my_printf("Ready");
			else if (tcb[tid].state == S_RUNNING)
				my_printf("Running");
			else if (tcb[tid].state == S_BLOCKED)
				my_printf("Blocked    %s", tcb[tid].event->name);
			else if (tcb[tid].state == S_EXIT)
				my_printf("Exiting");
			//swapTask();
		}
	}
	Semaphore* sem = semaphoreList;
	Semaphore** semLink = &semaphoreList;
	while (sem) {
		if (strncmp(sem->name, "task", 4) != 0) {
			printf("\nSemaphore: %s", sem->name);
			int i;
			for (i = 0; i < sem->taskCount; i++) {
				int tid = sem->block[i];
				if (tcb[tid].name) {
					printf("\n%4d/%-4d%20s%4d  ", tid, tcb[tid].parent, tcb[tid].name,
							tcb[tid].priority);
					if (tcb[tid].signal & mySIGSTOP)
						my_printf("Paused");
					else if (tcb[tid].state == S_NEW)
						my_printf("New");
					else if (tcb[tid].state == S_READY)
						my_printf("Ready");
					else if (tcb[tid].state == S_RUNNING)
						my_printf("Running");
					else if (tcb[tid].state == S_BLOCKED)
						my_printf("Blocked    %s", tcb[tid].event->name);
					else if (tcb[tid].state == S_EXIT)
						my_printf("Exiting");
					swapTask();
				}
			}
		}
		// move to next semaphore
		semLink = (Semaphore**) &sem->semLink;
		sem = (Semaphore*) sem->semLink;
	}

	swapTask();
	return 0;
} // end P2_listTasks

// ***********************************************************************
// ***********************************************************************
// list semaphores command
//
int match(char* mask, char* name) {
	int i, j;

	// look thru name
	i = j = 0;
	if (!mask[0])
		return 1;
	while (mask[i] && name[j]) {
		if (mask[i] == '*')
			return 1;
		if (mask[i] == '?')
			;
		else if ((mask[i] != toupper(name[j])) && (mask[i] != tolower(name[j])))
			return 0;
		i++;
		j++;
	}
	if (mask[i] == name[j])
		return 1;
	return 0;
} // end match

int P2_listSems(int argc, char* argv[])                         // listSemaphores
{
	Semaphore* sem = semaphoreList;
	while (sem) {
		if ((argc == 1) || match(argv[1], sem->name)) {
			printf("\n%20s  %c  %d  %s", sem->name, (sem->type ? 'C' : 'B'), sem->state,
					tcb[sem->taskNum].name);
		}
		sem = (Semaphore*) sem->semLink;
	}
	return 0;
} // end P2_listSems

// ***********************************************************************
// ***********************************************************************
// reset system
int P2_reset(int argc, char* argv[])                                            // reset
{
	longjmp(reset_context, POWER_DOWN_RESTART);
	// not necessary as longjmp doesn't return
	return 0;

} // end P2_reset

// ***********************************************************************
// ***********************************************************************
// kill task

int P2_killTask(int argc, char* argv[])                 // kill task
{
	int taskId = INTEGER(argv[1]);                                  // convert argument 1

			if ((taskId > 0) && tcb[taskId].name)// check for single task
			{
				my_printf("\nKill Task %d", taskId);
				tcb[taskId].state = S_EXIT;
				tcb[taskId].priority=100;
				enque(rq,taskId,&taskCount);
				return 0;
			}
			else if (taskId < 0)                                            // check for all tasks
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
			else                                                               // invalid argument
			{
				my_printf("\nIllegal argument or Invalid Task");
			}
			return 0;
		} // end P2_killTask

// ***********************************************************************
// ***********************************************************************
// signal command
void sem_signal(Semaphore* sem)         // signal
{
	if (sem) {
		printf("\nSignal %s", sem->name);
		SEM_SIGNAL(sem);
	} else
		my_printf("\nSemaphore not defined!");
	return;
} // end sem_signal

// ***********************************************************************
int P2_signal1(int argc, char* argv[])          // signal1
{          //P2_listTasks(1,NULL);
	printf("\n\n\n\n sig1-- first:%d second:%d", rq[0], rq[1]);
	//SWAP;
	if (!s1Sem || !s1Sem->name) {
		//printf("!!!!");
		s1Sem = createSemaphore("s1Sem", 0, 0);
	}
	//printf("%d",s1Sem->block[0]);
	SEM_SIGNAL(s1Sem);
	return 0;
} // end signal

int P2_signal2(int argc, char* argv[])          // signal2
{
	if (!s2Sem->name) {
		s2Sem = createSemaphore("s2Sem", 0, 0);
	}
	SEM_SIGNAL(s2Sem);
	return 0;
} // end signal

// ***********************************************************************
// ***********************************************************************
// signal task
//
#define COUNT_MAX       5
//
int signalTask(int argc, char* argv[]) {
	int count = 0;                                  // task variable
//printf("\n\n\n\n st-- first:%d second:%d",rq[0],rq[1]);

	// create a semaphore
	Semaphore** mySem = (!strcmp(argv[1], "s1Sem")) ? &s1Sem : &s2Sem;
	if ((!*mySem) || (!(*mySem)->name))
		*mySem = createSemaphore(argv[1], 0, 0);

	// loop waiting for semaphore to be signaled
	while (count < COUNT_MAX) { //printf("\n\n\n\n st-- first:%d second:%d third:%d",rq[0],rq[1],rq[2]);

		SEM_WAIT(*mySem);
		// wait for signal
		printf("\n%s  Task[%d], count=%d", tcb[curTask].name, curTask, ++count);
	}
	return 0;                                               // terminate task
} // end signalTask

// ***********************************************************************
// ***********************************************************************
// I'm alive task
int ImAliveTask(int argc, char* argv[]) {
	int i;                                                  // local task variable
	while (1) {
		printf("\n(%d) I'm Alive!", curTask);
		for (i = 0; i < 9000; i++) {
			swapTask();
		}

	}
	return 0;                                               // terminate task
} // end ImAliveTask

int P2_tenSecond(int argc, char* argv[]) {
	SWAP
	;
	while (1) {
		SEM_WAIT(tics10sec);

		SWAP
		;
		printf("\n\n\n\n 10 second of task %d ", curTask);
		P1_date_time(1, NULL );
	}

}
