// os345.c - OS Kernel
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the BYU CS345 projects.      **
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
#include <time.h>
#include <assert.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include "os345.h"
#include "os345lc3.h"
#include "os345fat.h"

// **********************************************************************
//	local prototypes
//
static void pollInterrupts(void);
static int scheduler(void);
static int dispatcher(int);

static void keyboard_isr(void);
static void timer_isr(void);

static int sysKillTask(int taskId);
static void initOS(void);

// **********************************************************************
// **********************************************************************
// global semaphores

Semaphore* semaphoreList;			// linked list of active semaphores

Semaphore* keyboard;				// keyboard semaphore
Semaphore* charReady;				// character has been entered
Semaphore* inBufferReady;			// input buffer ready semaphore

Semaphore* tics1sec;				// 1 second semaphore
Semaphore* tics10thsec;				// 1/10 second semaphore
Semaphore* tics10sec;				// 10 second semaphore

Semaphore* globalDelta;

// **********************************************************************
// **********************************************************************
// global system variables

TCB tcb[MAX_TASKS];					// task control block
Semaphore* taskSems[MAX_TASKS];		// task semaphore
jmp_buf k_context;					// context of kernel stack
jmp_buf reset_context;				// context of kernel stack
volatile void* temp;				// temp pointer used in dispatcher

int superMode;						// system mode
int curTask;						// current task #
long swapCount;						// number of re-schedule cycles
char inChar;						// last entered character
int charFlag;						// 0 => buffered input
int inBufIndx;						// input pointer into input buffer
char inBuffer[INBUF_SIZE + 1];		// character input buffer
Message messages[NUM_MESSAGES];		// process message buffers

int pollClock;						// current clock()
int lastPollClock;					// last pollClock
bool diskMounted;					// disk has been mounted

time_t oldTime1;					// old 1sec time
clock_t myClkTime;
clock_t myOldClkTime;
int* rq;							// ready priority queue

int HISTORY_MAX = 200;
char ** prevArgs;			// pointers to command line history
void initializeHistory();
void saveCommandInHistory(char * command);
int historyIndex;					// index to control history.
int historyViewer;
char blankBuffer[INBUF_SIZE + 1];
struct winsize w;

// Project 2 global variables
int ** readyQueue;
int * blockedQueue;
int tick;
int ten;

// Project 2 methods
void taskReadyToBlocked(int t);
void taskBlockedToReady(int t);
bool removeFromReadyQueue(int t);
void removeFromBlockedQueue(int t);

void initializeHistory() {
	historyIndex = 0;
	historyViewer = 0;
	prevArgs = (char**) malloc(sizeof(char*) * HISTORY_MAX);
	int z;
	for (z = 0; z < HISTORY_MAX; z++) {
		prevArgs[z] = malloc((INBUF_SIZE + 1) * sizeof(char));
	}
	for (z = 0; z < INBUF_SIZE; z++)
		blankBuffer[z] = 0;
}

void freeHistory() {
	int z;
	for (z = 0; z < HISTORY_MAX; z++) {
		free(prevArgs[z]);
	}
	free(prevArgs);
}

void saveCommandInHistory(char * command) {
	if (historyIndex >= HISTORY_MAX) {		// shift history back one to replace old entries.
		int j;
		for (j = 0; j < HISTORY_MAX; j++) {
			int i;
			for (i = 0; i < INBUF_SIZE; i++)
				prevArgs[j][i] = 0;
			if (j != (HISTORY_MAX - 1))
				strcpy(prevArgs[j], prevArgs[j + 1]);
		}
		historyIndex = historyIndex - 1;
	}
	strcpy(prevArgs[historyIndex], command);
	historyIndex = historyIndex + 1;
	historyViewer = historyIndex;
}

//  project 3 functions:

// Delta Clock Variables
clockEvent* deltaClock = 0;

#define ONE_TENTH_SEC           (CLOCKS_PER_SEC/10)
static const int PERIODIC = 1;
static const int NOT_PERIODIC = 0;

int insertDeltaClock(int time, Semaphore * semId, int periodic);
int deleteClockEvent(Semaphore* semId);
int tickDeltaClock();
int listDeltaClock();

int testDeltaClock(int argc, char* argv[]);
int deltaClockTestTask(int argc, char* argv[]);

// **********************************************************************
// **********************************************************************
// OS startup
//
// 1. Init OS
// 2. Define reset longjmp vector
// 3. Define global system semaphores
// 4. Create CLI task
// 5. Enter scheduling/idle loop
//
int main(int argc, char* argv[]) {
	// All the 'powerDown' invocations must occur in the 'main'
	// context in order to facilitate 'killTask'.  'killTask' must
	// free any stack memory associated with current known tasks.  As
	// such, the stack context must be one not associated with a task.
	// The proper method is to longjmp to the 'reset_context' that
	// restores the stack for 'main' and then invoke the 'powerDown'
	// sequence.

	// save context for restart (a system reset would return here...)
	int resetCode = setjmp(reset_context);
	superMode = TRUE;						// supervisor mode

	switch (resetCode) {
	case POWER_DOWN_QUIT:				// quit
		powerDown(0);
		printf("\nGoodbye!!");
		return 0;

	case POWER_DOWN_RESTART:			// restart
		powerDown(resetCode);
		printf("\nRestarting system...\n");

	case POWER_UP:						// startup
		break;

	default:
		printf("\nShutting down due to error %d", resetCode);
		powerDown(resetCode);
		return 0;
	}

	// output header message
	printf("%s", STARTUP_MSG);

	// initalize OS
	initOS();

	// create global/system semaphores here
	//?? vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

	charReady = createSemaphore("charReady", BINARY, 0);
	inBufferReady = createSemaphore("inBufferReady", BINARY, 0);
	keyboard = createSemaphore("keyboard", BINARY, 1);
	tics1sec = createSemaphore("tics1sec", BINARY, 0);
	tics10thsec = createSemaphore("tics10thsec", BINARY, 0);
	tics10sec = createSemaphore("tics10sec", COUNTING, 0);
	globalDelta = createSemaphore("globalDelta", BINARY, 1);

	//?? ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	// schedule CLI task
	createTask("myShell",			// task name
			P1_shellTask,			// task
			MED_PRIORITY,			// task priority
			argc,					// task arg count
			argv);					// task argument pointers

	// HERE WE GO................

	// Scheduling loop
	// 1. Check for asynchronous events (character inputs, timers, etc.)
	// 2. Choose a ready task to schedule
	// 3. Dispatch task
	// 4. Loop (forever!)

	while (1)									// scheduling loop
	{
		// check for character / timer interrupts
		pollInterrupts();

		// schedule highest priority ready task
		if ((curTask = scheduler()) < 0)
			continue;

		// dispatch curTask, quit OS if negative return
		if (dispatcher(curTask) < 0)
			break;

	}											// end of scheduling loop

	// exit os
	longjmp(reset_context, POWER_DOWN_QUIT);
	return 0;
} // end main

// **********************************************************************
// keyboard interrupt service routine
//
static void keyboard_isr() {
	// assert system mode
	assert("keyboard_isr Error" && superMode);

	semSignal(charReady);					// SIGNAL(charReady) (No // SWAP)
	if (charFlag == 0) {
		switch (inChar) {
		case '\r':
		case '\n': {
			inBufIndx = 0;				// EOL, signal line ready
			semSignal(inBufferReady);	// SIGNAL(inBufferReady)
			break;
		}
		case 0x06:						// ^F
		case 0x04:						// ^D
		{
			char * toShow = malloc(sizeof(char) * INBUF_SIZE);
			int i;
			if (inChar == 0x04) {
				if (historyViewer <= 0) {						// history empty
					for (i = 0; i < INBUF_SIZE; i++)
						toShow[i] = 0;		// clear toShow
					free(toShow);
					break;
				}
				strcpy(toShow, prevArgs[historyViewer - 1]);	// copy one back
				historyViewer = historyViewer - 1;
				if (historyViewer <= 0) {
					historyViewer = 0;						// assert cant push index
				}											// out of bounds (below 0)
			} else {
				if (historyViewer >= historyIndex - 1) {	// assert cant go into future history
					for (i = 0; i < INBUF_SIZE; i++)
						toShow[i] = 0;		// clear toShow
					free(toShow);
					break;
				}
				strcpy(toShow, prevArgs[historyViewer + 1]);		// copy one forward
				historyViewer = historyViewer + 1;
			}
			int oldLen = strlen(inBuffer);
			for (i = 0; i < INBUF_SIZE; i++)
				inBuffer[i] = 0;	// clear inBuffer
			strcpy(inBuffer, toShow);						// set inBuffer
			inBufIndx = strlen(inBuffer) - 1;				// set inBufIndx
			for (i = 0; i <= oldLen + 2; i++)
				blankBuffer[i] = ' '; // hide old buffer with spaces.
			printf("\r%s", blankBuffer);						// clear stdout
			printf("\r%ld>>%s", swapCount - 1, toShow);		// print history
			for (i = 0; i < INBUF_SIZE; i++)
				toShow[i] = 0;		// clear toShow
			for (i = 0; i < strlen(inBuffer); i++)
				blankBuffer[i] = 0;
			free(toShow);

			break;
		}
		case 0x12:						// ^R
		{
			inBufIndx = 0;
			inBuffer[0] = 0;
			sigSignal(-1, mySIGCONT);	// send mySIGCONT to all tasks
			int taskId;
			for (taskId = 0; taskId < MAX_TASKS; taskId++) {
				if (tcb[taskId].signal & mySIGTSTP)
					tcb[taskId].signal &= ~mySIGTSTP;
				if (tcb[taskId].signal & mySIGSTOP)
					tcb[taskId].signal &= ~mySIGSTOP;
			}
			break;
		}
		case 0x17:						// ^w
		{
			inBufIndx = 0;
			inBuffer[0] = 0;
			sigSignal(-1, mySIGTSTP);	// send mySIGTSTP to all tasks
			break;
		}
		case 0x18:						// ^x
		{
			inBufIndx = 0;
			inBuffer[0] = 0;
			sigSignal(0, mySIGINT);		// interrupt task 0
			break;
		}

		case 0x08:
		case 0x7f: {
			if (inBufIndx > 0) {
				inBufIndx--;
				inBuffer[inBufIndx] = 0;
				printf("\b \b");
			}
			break;
		}

		default: {
			inBuffer[inBufIndx++] = inChar;
			inBuffer[inBufIndx] = 0;
			printf("%c", inChar);		// echo character
		}
		}
	} else {
		// single character mode
		inBufIndx = 0;
		inBuffer[inBufIndx] = 0;
	}
	return;
} // end keyboard_isr

// **********************************************************************
// timer interrupt service routine
//
static void timer_isr() {
	time_t currentTime;						// current time

	// assert system mode
	assert("timer_isr Error" && superMode);

	// capture current time
	time(&currentTime);

	// one second timer
	if ((currentTime - oldTime1) >= 1) {
		// signal 1 second
		semSignal(tics1sec);
		oldTime1 += 1;
		ten = ten + 1;
		// printf("\nONE SECOND! and tics10sec = %d",tics10sec->state);
	}

	// sample fine clock
	myClkTime = clock();
	if ((myClkTime - myOldClkTime) >= ONE_TENTH_SEC) {
		myOldClkTime = myOldClkTime + ONE_TENTH_SEC;   // update old
		semSignal(tics10thsec);
		int tenths;
		tickDeltaClock();
	}

	// ?? add other timer sampling/signaling code here for project 2
	if (ten >= 10) {
		ten = 0;
		semSignal(tics10sec);
	}

	return;
} // end timer_isr

// **********************************************************************
// **********************************************************************
// simulate asynchronous interrupts by polling events during idle loop
//
static void pollInterrupts(void) {
	// check for task monopoly
	pollClock = clock();
	assert("Timeout" && ((pollClock - lastPollClock) < MAX_CYCLES));
	lastPollClock = pollClock;

	// check for keyboard interrupt
	if ((inChar = GET_CHAR) > 0) {
		keyboard_isr();
	}

	// timer interrupt
	timer_isr();

	// delta clock
	//tickDeltaClock();

	return;
} // end pollInterrupts

// **********************************************************************
// **********************************************************************
// scheduler
//
static int scheduler() {
	int nextTask = 0;
	// ?? Design and implement a scheduler that will select the next highest
	// ?? priority ready task to pass to the system dispatcher.

	// ?? WARNING: You must NEVER call // SWAPTask() from within this function
	// ?? or any function that it calls.  This is because // SWAPping is
	// ?? handled entirely in the // SWAPTask function, which, in turn, may
	// ?? call this function.  (ie. You would create an infinite loop.)

	// ?? Implement a round-robin, preemptive, prioritized scheduler.

	// ?? This code is simply a round-robin scheduler and is just to get
	// ?? you thinking about scheduling.  You must implement code to handle
	// ?? priorities, clean up dead tasks, and handle semaphores appropriately.

	int pri, tsk;
	tick = 0;	// set tick to 0 to not print debug info
	bool taskFound = FALSE;
	for (pri = MAX_PRIORITY; pri >= 0; pri--) { // start at highest priority
		if (taskFound)
			break;
		for (tsk = 0; tsk <= MAX_TASKS; tsk++) { // do FIFO
			if ((readyQueue[pri][tsk]) == -1) {
				// no task
			} else {
				// for debug, print next task
				//if(tick) {
				//printf("\nTask %d at priority %d is scheduled.",readyQueue[pri][tsk],pri,tsk);
				//}
				nextTask = readyQueue[pri][tsk];	// set next task
				int back;							// shift queue to enable fifo
				for (back = tsk; back <= MAX_TASKS; back++) {
					readyQueue[pri][back] = readyQueue[pri][back + 1];
					if (readyQueue[pri][back] == -1) {
						readyQueue[pri][back] = nextTask;
						break;
					}
				}
				taskFound = TRUE;
				break;
			}
		}
	}

	// schedule next task
	// nextTask = ++curTask;

	// mask sure nextTask is valid
	if (!tcb[nextTask].name)
		return -1;

	if (tcb[nextTask].signal & mySIGSTOP)
		return -1;

	return nextTask;
} // end scheduler

// **********************************************************************
// **********************************************************************
// dispatch curTask
//
static int dispatcher(int curTask) {
	int result;

	// schedule task
	switch (tcb[curTask].state) {
	case S_NEW: {
		// new task
		printf("\nNew Task[%d] %s", curTask, tcb[curTask].name);
		tcb[curTask].state = S_RUNNING;	// set task to run state

		// save kernel context for task // SWAP's
		if (setjmp(k_context)) {
			superMode = TRUE;					// supervisor mode
			break;								// context switch to next task
		}

		// move to new task stack (leave room for return value/address)
		temp = (int*) tcb[curTask].stack + (STACK_SIZE - 8);
		SET_STACK(temp);
		superMode = FALSE;						// user mode

		// begin execution of new task, pass argc, argv
		result = (*tcb[curTask].task)(tcb[curTask].argc, tcb[curTask].argv);

		// task has completed
		if (result)
			printf("\nTask[%d] returned %d", curTask, result);
		else
			printf("\nTask[%d] returned %d", curTask, result);
		tcb[curTask].state = S_EXIT;			// set task to exit state

		// return to kernal mode
		longjmp(k_context, 1);					// return to kernel
	}

	case S_READY: {
		tcb[curTask].state = S_RUNNING;			// set task to run
	}

	case S_RUNNING: {
		if (setjmp(k_context)) {
			// // SWAP executed in task
			superMode = TRUE;					// supervisor mode
			break;								// return from task
		}
		if (tcb[curTask].signal) {
			if (tcb[curTask].signal & mySIGINT) {
				tcb[curTask].signal &= ~mySIGINT;
				(*tcb[curTask].sigIntHandler)();
			}
			if (tcb[curTask].signal & mySIGCONT) {
				tcb[curTask].signal &= ~mySIGCONT;
				(*tcb[curTask].sigContHandler)();
			}
			if (tcb[curTask].signal & mySIGTERM) {
				tcb[curTask].signal &= ~mySIGTERM;
				(*tcb[curTask].sigTermHandler)();
			}
			if (tcb[curTask].signal & mySIGTSTP) {
				tcb[curTask].signal &= ~mySIGTSTP;
				(*tcb[curTask].sigTstpHandler)();
			}
		}

		longjmp(tcb[curTask].context, 3); 		// restore task context
	}

	case S_BLOCKED: {
		// ?? Could check here to unblock task
		break;
	}

	case S_EXIT: {
		if (curTask == 0)
			return -1;			// if CLI, then quit scheduler
		// release resources and kill task
		sysKillTask(curTask);					// kill current task
		break;
	}

	default: {
		printf("Unknown Task[%d] State", curTask);
		longjmp(reset_context, POWER_DOWN_ERROR);
	}
	}
	return 0;
} // end dispatcher

// **********************************************************************
// **********************************************************************
// Do a context switch to next task.

// 1. If scheduling task, return (setjmp returns non-zero value)
// 2. Else, save current task context (setjmp returns zero value)
// 3. Set current task state to READY
// 4. Enter kernel mode (longjmp to k_context)

void swapTask() {
	assert("// SWAP Error" && !superMode);
	// assert user mode

	// increment // SWAP cycle counter
	swapCount++;

	// either save current task context or schedule task (return)
	if (setjmp(tcb[curTask].context)) {
		superMode = FALSE;					// user mode
		return;
	}

	// context switch - move task state to ready
	if (tcb[curTask].state == S_RUNNING)
		tcb[curTask].state = S_READY;

	// move to kernel mode (reschedule)
	longjmp(k_context, 2);
} // end // SWAPTask

// **********************************************************************
// **********************************************************************
// system utility functions
// **********************************************************************
// **********************************************************************

// **********************************************************************
// **********************************************************************
// initialize operating system
static void initOS() {
	int i;

	// make any system adjustments (for unblocking keyboard inputs)
	INIT_OS

	// reset system variables
	curTask = 0;						// current task #
	swapCount = 0;						// number of scheduler cycles
	inChar = 0;							// last entered character
	charFlag = 0;						// 0 => buffered input
	inBufIndx = 0;						// input pointer into input buffer
	semaphoreList = 0;					// linked list of active semaphores
	diskMounted = 0;					// disk has been mounted

	// malloc ready queue
	rq = (int*) malloc(MAX_TASKS * sizeof(int));

	// capture current time
	lastPollClock = clock();			// last pollClock
	time(&oldTime1);

	// init system tcb's
	for (i = 0; i < MAX_TASKS; i++) {
		tcb[i].name = NULL;				// tcb
		taskSems[i] = NULL;				// task semaphore
	}

	// initalize message buffers
	for (i = 0; i < NUM_MESSAGES; i++) {
		messages[i].to = -1;
	}

	// init tcb
	for (i = 0; i < MAX_TASKS; i++) {
		tcb[i].name = NULL;
	}

	// initialize lc-3 memory
	initLC3Memory(LC3_MEM_FRAME, 0xF800 >> 6);

	// ?? initialize all execution queues
	int i2, j;

	// initialize ready queue an int[MAX_PRIORITY][MAX_TASKS]
	readyQueue = malloc((MAX_PRIORITY + 1) * sizeof(int*));
	for (i2 = 0; i2 <= MAX_PRIORITY; i2++) {
		readyQueue[i2] = malloc((MAX_TASKS + 1) * sizeof(int));
		for (j = 0; j <= MAX_TASKS; j++) {
			readyQueue[i2][j] = -1;
		}
	}
	tick = 1;

	// initialize blocked queue an int[MAX_TASKS]
	blockedQueue = malloc((MAX_TASKS + 1) * sizeof(int));
	for (i2 = 0; i2 <= MAX_TASKS; i2++) {
		blockedQueue[i2] = -1;
	}

	return;
} // end initOS

// **********************************************************************
// **********************************************************************
// Causes the system to shut down. Use this for critical errors
void powerDown(int code) {
	int i;
	printf("\nPowerDown Code %d", code);

	// release all system resources.
	printf("\nRecovering Task Resources...");

	// kill all tasks
	for (i = MAX_TASKS - 1; i >= 0; i--)
		if (tcb[i].name)
			sysKillTask(i);

	// delete all semaphores
	while (semaphoreList)
		deleteSemaphore(&semaphoreList);

	// free ready queue
	free(rq);

	// release any other system resources
	int i2;
	// free ready queue an int[MAX_PRIORITY][MAX_TASKS]

	for (i2 = 0; i2 <= MAX_PRIORITY; i2++) {
		// readyQueue[i2] = malloc(MAX_TASKS * sizeof(int));
		free(readyQueue[i2]);
	}
	//readyQueue = malloc(MAX_PRIORITY * sizeof(int*));
	free(readyQueue);

	// free blocked queue
	free(blockedQueue);

	int z;
	for (z = 0; z < HISTORY_MAX; z++) {
		free(prevArgs[z]);
	}
	free(prevArgs);

	// ?? deltaclock (project 3)

	RESTORE_OS
	return;
} // end powerDown

// **********************************************************************
// **********************************************************************
//	Signal handlers
//
int sigAction(void (*sigHandler)(void), int sig) {
	switch (sig) {
	case mySIGINT: {
		tcb[curTask].sigIntHandler = sigHandler;		// mySIGINT handler
		return 0;
	}
	case mySIGTERM: {
		tcb[curTask].sigTermHandler = sigHandler;		// mySIGTERM handler
		return 0;
	}
	case mySIGTSTP: {
		tcb[curTask].sigTstpHandler = sigHandler;		// mySIGSTP handler
		return 0;
	}
	case mySIGCONT: {
		tcb[curTask].sigContHandler = sigHandler;		// mySIGCONT handler
		return 0;
	}
	}
	return 1;
}

// **********************************************************************
//	sigSignal - send signal to task(s)
//
//	taskId = task (-1 = all tasks)
//	sig = signal
//
int sigSignal(int taskId, int sig) {
	// check for task
	if ((taskId >= 0) && tcb[taskId].name) {
		tcb[taskId].signal |= sig;
		return 0;
	} else if (taskId == -1) {
		for (taskId = 0; taskId < MAX_TASKS; taskId++) {
			sigSignal(taskId, sig);
		}
		return 0;
	}
	// error
	return 1;
}

// **********************************************************************
// **********************************************************************
//	Default signal handlers
//
void defaultSigIntHandler(void)			// task mySIGINT handler
{
	printf("\ndefaultSigIntHandler");
	return;
}
void defaultSigTstpHandler(void)			// task mySIGTSTP handler
{
	printf("\ndefaultSigTstpHandler");
	return;
}
void defaultSigContHandler(void)			// task mySIGCont handler
{
	printf("\ndefaultSigContHandler");
	return;
}
void defaultSigTermHandler(void)			// task mySIGTerm handler
{
	printf("\ndefaultSigTermHandler");
	return;
}

// **********************************************************************
// **********************************************************************
// create task
int createTask(char* name,						// task name
		int (*task)(int, char**),	// task address
		int priority,				// task priority
		int argc,					// task argument count
		char* argv[])				// task argument pointers
{
	int tid;

	// find an open tcb entry slot
	for (tid = 0; tid < MAX_TASKS; tid++) {
		if (tcb[tid].name == 0) {
			char buf[8];

			// create task semaphore
			if (taskSems[tid])
				deleteSemaphore(&taskSems[tid]);
			sprintf(buf, "task%d", tid);
			taskSems[tid] = createSemaphore(buf, 0, 0);
			taskSems[tid]->taskNum = 0;	// assign to shell

			// copy task name
			tcb[tid].name = (char*) malloc(strlen(name) + 1);
			strcpy(tcb[tid].name, name);

			// set task address and other parameters
			tcb[tid].task = task;			// task address
			tcb[tid].state = S_NEW;			// NEW task state
			tcb[tid].priority = priority;	// task priority
			tcb[tid].parent = curTask;		// parent
			tcb[tid].argc = argc;			// argument count

			// ?? malloc new argv parameters
			char **newArgv = malloc(sizeof(char*) * argc);
			int z;
			for (z = 0; z < argc; z++) {
				newArgv[z] = argv[z];
			}
			tcb[tid].argv = newArgv;			// argument pointers

			tcb[tid].event = 0;				// suspend semaphore
			tcb[tid].RPT = 0;					// root page table (project 5)
			tcb[tid].cdir = CDIR;			// inherit parent cDir (project 6)

			// signals
			tcb[tid].signal = 0;
			if (tid) {
				// inherit parent signal handlers
				tcb[tid].sigIntHandler = tcb[curTask].sigIntHandler;	// mySIGINT handler
				tcb[tid].sigTstpHandler = tcb[curTask].sigTstpHandler;	// mySIGTSTP handler
				tcb[tid].sigContHandler = tcb[curTask].sigContHandler;	// mySIGCONT handler
				tcb[tid].sigTermHandler = tcb[curTask].sigTermHandler;	// mySIGTERM handler
			} else {
				// otherwise use defaults
				tcb[tid].sigIntHandler = defaultSigIntHandler;			// task mySIGINT handler
				tcb[tid].sigTstpHandler = defaultSigTstpHandler;		// task mySIGTSTP handler
				tcb[tid].sigContHandler = defaultSigContHandler;		// task mySIGCONT handler
				tcb[tid].sigTermHandler = defaultSigTermHandler;		// task mySIGTERM handler
			}

			// Each task must have its own stack and stack pointer.
			tcb[tid].stack = malloc(STACK_SIZE * sizeof(int));

			// may require inserting task into "ready" queue
			int q = 0;
			while (readyQueue[priority][q++] != -1) {
			}
			q = q - 1;
			readyQueue[priority][q] = tid;
			// printf("\nentered task id %d into slot [%d][%d]",tid,priority,q);

			if (tid)
				swapTask();				// do context switch (if not cli)
			return tid;							// return tcb index (curTask)
		}
	}
	// tcb full!
	return -1;
} // end createTask

// **********************************************************************
// **********************************************************************
// kill task
//
//	taskId == -1 => kill all non-shell tasks
//
static void exitTask(int taskId);
int killTask(int taskId) {
	int tid;
	assert("killTask Error" && tcb[taskId].name);

	if (taskId != 0)			// don't terminate shell
			{
		if (taskId < 0)		// kill all tasks
				{
			for (tid = 0; tid < MAX_TASKS; tid++) {
				if (tcb[tid].name)
					exitTask(tid);
			}
		} else {
			// terminate individual task
			exitTask(taskId);	// kill individual task
		}
	}
	if (!superMode)
		SWAP
	;
	return 0;
} // end killTask

static void exitTask(int taskId) {
	assert("exitTaskError" && tcb[taskId].name);

	// 1. find task in system queue
	// 2. if blocked, unblock (handle semaphore)
	if (tcb[taskId].state == S_BLOCKED) {
		semSignal(tcb[taskId].event);
	}
	// 3. set state to exit

	// free memory
	free(tcb[taskId].argv);

	tcb[taskId].state = S_EXIT;				// EXIT task state

	return;
} // end exitTask

// **********************************************************************
// system kill task
//
static int sysKillTask(int taskId) {
	Semaphore* sem = semaphoreList;
	Semaphore** semLink = &semaphoreList;

	// assert that you are not pulling the rug out from under yourself!
	assert("sysKillTask Error" && tcb[taskId].name && superMode);
	// printf("\nKill Task %s", tcb[taskId].name);

	// signal task terminated
	semSignal(taskSems[taskId]);

	// look for any semaphores created by this task
	while ((sem = *semLink)) {
		if (sem->taskNum == taskId) {
			// semaphore found, delete from list, release memory
			deleteSemaphore(semLink);
		} else {
			// move to next semaphore
			semLink = (Semaphore**) &sem->semLink;
		}
	}

	// delete task from system queues

	// delete from ready queue
	removeFromReadyQueue(taskId);

	// delete from blocked queue
	removeFromBlockedQueue(taskId);

	free(tcb[taskId].name);
	free(tcb[taskId].stack);
	tcb[taskId].name = 0;			// release tcb slot

	return 0;
} // end killTask

void removeFromBlockedQueue(int t) {
	// printf("\nremoving task %d from blocked.",t);
	int j;
	bool found = FALSE;
	for (j = 0; j < MAX_TASKS; j++) {	// remove from blocked queue
		if (blockedQueue[j] == t || found) {
			found = TRUE;
			blockedQueue[j] = blockedQueue[j + 1];	// shift queue
			// printf("\ntask %d is now at %d",blockedQueue[j],j);
			if (blockedQueue[j] == -1)
				break;
		}
	}
}

bool removeFromReadyQueue(int t) {
	int pri, tsk;
	bool taskFound = FALSE;
	for (pri = MAX_PRIORITY; pri >= 0; pri--) { // start at highest priority
		if (taskFound)
			break;
		for (tsk = 0; tsk <= MAX_TASKS; tsk++) { // do FIFO
			if ((readyQueue[pri][tsk]) == t) {		// found.
				int back;							// shift queue
				for (back = tsk; back <= MAX_TASKS; back++) {
					readyQueue[pri][back] = readyQueue[pri][back + 1];
					if (readyQueue[pri][back] == -1) {
						break;
					}
				}
				taskFound = TRUE;
				break;
			}
		}
	}
	return taskFound;
}

void taskBlockedToReady(int t) {
	removeFromBlockedQueue(t);
	// insert into ready queue
	int q = 0;
	while (readyQueue[tcb[t].priority][q++] != -1) {
	}
	q = q - 1;
	readyQueue[tcb[t].priority][q] = t;
	// printf("\nreentered task id %d into slot [%d][%d] from blockedQueue",t,tcb[t].priority,q);

}

void taskReadyToBlocked(int t) {
	// ?? move task from ready queue to blocked queue
	if (removeFromReadyQueue(t)) { // move to blocked queue
		int j = 0;
		while (blockedQueue[j++] != -1) {
		};	// insert in at end of blocked
		j = j - 1;
		blockedQueue[j] = t;
		// printf("\nput %d in blocked queue at %d\n",t,j);
	}
}

// **********************************************************************
// **********************************************************************
// signal semaphore
//
//	if task blocked by semaphore, then clear semaphore and wakeup task
//	else signal semaphore
//
void semSignal(Semaphore* s) {
	int i;
	// assert there is a semaphore and it is a legal type
	assert("semSignal Error" && s && ((s->type == 0) || (s->type == 1)));

	// check semaphore type
	if (s->type == 0) {
		// binary semaphore
		// look through tasks for one suspended on this semaphore

		for (i = 0; i < MAX_TASKS; i++)	// look for suspended task
				{
			if (tcb[i].event == s) {
				s->state = 0;				// clear semaphore
				tcb[i].event = 0;			// clear event pointer
				tcb[i].state = S_READY;		// unblock task

				// move task from blocked to ready queue
				taskBlockedToReady(i);

				if (!superMode)
					swapTask();
				return;
			}
		}
		// nothing waiting on semaphore, go ahead and just signal
		s->state = 1;						// nothing waiting, signal
		if (!superMode)
			swapTask();
		return;
	} else {
		// counting semaphore
		s->state = s->state + 1;
		if (s->state == 1) {
			// unblock a task waiting on this counting sem.
			for (i = 0; i < MAX_TASKS; i++) { // look for suspended task
				if (blockedQueue[i] == -1) {
					return;
				}
				if (tcb[blockedQueue[i]].event == s) {
					tcb[blockedQueue[i]].event = 0;			// clear event pointer
					tcb[blockedQueue[i]].state = S_READY;		// unblock task

					// move task from blocked to ready queue
					taskBlockedToReady(blockedQueue[i]);

					if (!superMode)
						swapTask();
					return;
				}
			}
		} // end semSignal
	}
}

// **********************************************************************
// **********************************************************************
// wait on semaphore
//
//	if semaphore is signaled, return immediately
//	else block task
//
int semWait(Semaphore* s) {
	assert("semWait Error" && s);
	// assert semaphore
	assert("semWait Error" && ((s->type == 0) || (s->type == 1)));
	// assert legal type
	assert("semWait Error" && !superMode);
	// assert user mode

	// check semaphore type
	if (s->type == 0) {
		// binary semaphore
		// if state is zero, then block task

		if (s->state == 0) {
			tcb[curTask].event = s;		// block task
			tcb[curTask].state = S_BLOCKED;

			// Move task from ready to blocked queue
			taskReadyToBlocked(curTask);

			swapTask();						// reschedule the tasks
			return 1;
		}
		// state is non-zero (semaphore already signaled)
		s->state = 0;						// reset state, and don't block
		return 0;
	} else {
		// counting semaphore
		s->state = s->state - 1;
		if (s->state < 0) {
			s->state = 0;				// set lower limit to 0
			tcb[curTask].event = s;		// block task
			tcb[curTask].state = S_BLOCKED;

			// Move task from ready to blocked queue
			taskReadyToBlocked(curTask);

			swapTask();						// reschedule the tasks
			return 1;
		}
		return 0;	// don't block
	}
} // end semWait

// **********************************************************************
// **********************************************************************
// try to wait on semaphore
//
//	if semaphore is signaled, return 1
//	else return 0
//
int semTryLock(Semaphore* s) {
	assert("semTryLock Error" && s);
	// assert semaphore
	assert("semTryLock Error" && ((s->type == 0) || (s->type == 1)));
	// assert legal type
	assert("semTryLock Error" && !superMode);
	// assert user mode

	// check semaphore type
	if (s->type == 0) {
		// binary semaphore
		// if state is zero, then block task
		if (s->state == 0) {
			return 0;
		}
		// state is non-zero (semaphore already signaled)
		s->state = 0;						// reset state, and don't block
		return 1;
	} else {
		// counting semaphore
		// implement counting semaphore
		s->state = s->state - 1;
		if (s->state < 0) {					// ?? do you have to block in tryLock?
			s->state = 0;					// set lower limit to 0
			// tcb[curTask].event = s;		// block task
			// tcb[curTask].state = S_BLOCKED;

			// Move task from ready to blocked queue
			// taskReadyToBlocked(curTask);

			swapTask();						// reschedule the tasks
			return 0;
		} else {
			return 1;
		}
	}
} // end semTryLock

// **********************************************************************
// **********************************************************************
// Create a new semaphore.
// Use heap memory (malloc) and link into semaphore list (Semaphores)
// 	name = semaphore name
//		type = binary (0), counting (1)
//		state = initial semaphore state
// Note: memory must be released when the OS exits.
//
Semaphore* createSemaphore(char* name, int type, int state) {
	Semaphore* sem = semaphoreList;
	Semaphore** semLink = &semaphoreList;

	// assert semaphore is binary or counting
	assert("createSemaphore Error" && ((type == 0) || (type == 1)));
	// assert type is validate

	// look for duplicate name
	while (sem) {
		if (!strcmp(sem->name, name)) {
			printf("\nSemaphore %s already defined", sem->name);

			// ?? What should be done about duplicate semaphores ??
			// semaphore found - change to new state
			sem->type = type;					// 0=binary, 1=counting
			sem->state = state;				// initial semaphore state
			sem->taskNum = curTask;			// set parent task #
			return sem;
		}
		// move to next semaphore
		semLink = (Semaphore**) &sem->semLink;
		sem = (Semaphore*) sem->semLink;
	}

	// allocate memory for new semaphore
	sem = (Semaphore*) malloc(sizeof(Semaphore));

	// set semaphore values
	sem->name = (char*) malloc(strlen(name) + 1);
	strcpy(sem->name, name);				// semaphore name
	sem->type = type;							// 0=binary, 1=counting
	sem->state = state;						// initial semaphore state
	sem->taskNum = curTask;					// set parent task #

	// prepend to semaphore list
	sem->semLink = (struct semaphore*) semaphoreList;
	semaphoreList = sem;						// link into semaphore list
	return sem;									// return semaphore pointer
} // end createSemaphore

// **********************************************************************
// **********************************************************************
// Delete semaphore and free its resources
//
bool deleteSemaphore(Semaphore** semaphore) {
	Semaphore* sem = semaphoreList;
	Semaphore** semLink = &semaphoreList;

	// assert there is a semaphore
	assert("deleteSemaphore Error" && *semaphore);

	// look for semaphore
	while (sem) {
		if (sem == *semaphore) {
			// semaphore found, delete from list, release memory
			*semLink = (Semaphore*) sem->semLink;

			// free the name array before freeing semaphore
			printf("\ndeleteSemaphore(%s)", sem->name);

			// ?? free all semaphore memory
			free(sem->name);
			free(sem);

			return TRUE;
		}
		// move to next semaphore
		semLink = (Semaphore**) &sem->semLink;
		sem = (Semaphore*) sem->semLink;
	}

	// could not delete
	return FALSE;
} // end deleteSemaphore

// **********************************************************************
// **********************************************************************
// post a message to the message buffers
//
int postMessage(int from, int to, char* msg) {
	int i;
	// insert message in open slot
	for (i = 0; i < NUM_MESSAGES; i++) {
		if (messages[i].to == -1) {
			//printf("\n(%d) Send from %d to %d: (%s)", i, from, to, msg);
			messages[i].from = from;
			messages[i].to = to;
			messages[i].msg = malloc(strlen(msg) + 1);
			strcpy(messages[i].msg, msg);
			return 1;
		}
	}
	printf("\n  **Message buffer full!  Message (%d,%d: %s) not sent.", from, to, msg);
	return 0;
} // end postMessage

// **********************************************************************
// **********************************************************************
// retrieve a message from the message buffers
//
int getMessage(int from, int to, Message* msg) {
	int i;
	for (i = 0; i < NUM_MESSAGES; i++) {
		if ((messages[i].to == to) && ((messages[i].from == from) || (from == -1))) {
			// get copy of message
			msg->from = messages[i].from;
			msg->to = messages[i].to;
			msg->msg = messages[i].msg;

			// roll list down
			for (; i < NUM_MESSAGES - 1; i++) {
				messages[i] = messages[i + 1];
				if (messages[i].to < 0)
					break;
			}
			messages[i].to = -1;
			return 0;
		}
	}
	printf("\n  **No message from %d to %d", from, to);
	return 1;
} // end getMessage

// **********************************************************************
// **********************************************************************
// read current time
//
char* myTime(char* svtime) {
	time_t cTime;						// current time

	time(&cTime);						// read current time
	strcpy(svtime, asctime(localtime(&cTime)));
	svtime[strlen(svtime) - 1] = 0;		// eliminate nl at end
	return svtime;
} // end myTime

/**
 * copyClock: makes a hard copy of clockEvent.
 * @other: the clockEvent to be copied.
 * @return: the copy of other.
 */
clockEvent* copyClock(clockEvent* other) {
	// SWAP
	clockEvent * copy = (clockEvent*) malloc(sizeof(clockEvent));
	// SWAP
	copy->init_value = other->init_value;
	// SWAP
	copy->next = other->next;
	// SWAP
	copy->periodic = other->periodic;
	// SWAP
	copy->sem = other->sem;
	// SWAP
	copy->time = other->time;
	// SWAP
	return copy;
}

/**
 * findEventBefore:  will search the deltaClock linked list and return the
 * pointer to the event that the event to be inserted will be placed after.
 * @toBeInserted: the event that will be inserted.
 * @return: the event that is before the event to be inserted
 *
 * This will also keep track of the times in all the other events and subtract
 * the time in the event that will be inserted by the appropriate ammount.
 *
 *
 */
clockEvent * findEventBefore(clockEvent * toBeInserted) {

	// define an int that will keep track of total time of all events that
	// have been checked against an int that is the time to be inserted
	// SWAP
	int totalTime = 0;
	// SWAP
	int timeToBeat = toBeInserted->time;
	// SWAP

	// define two pointers that will keep the current and previous events
	clockEvent * current = deltaClock;
	// SWAP
	clockEvent * prev = deltaClock;
	// SWAP

	// start counting totaltime
	totalTime = totalTime + current->time;
	// SWAP

	// while there is a next event, advance the current pointer, add the time
	// , check it agianst the time to beat.  If it beats it, subtract the
	// currently added time and break.  the pointer to prev will be the
	// clockEvent that will be before the one to be inserted.
	while (current->next != NULL ) {
		// SWAP
		current = current->next;
		// SWAP
		totalTime = totalTime + current->time;
		// SWAP
		if (totalTime > timeToBeat) {
			// SWAP
			totalTime = totalTime - current->time;
			// SWAP
			break;
		}
		// SWAP
		prev = current;
	}
	// SWAP

	// decrement the time of the one to be inserted so that it synchronizes
	// with the total delta clock.
	toBeInserted->time = timeToBeat - totalTime;
	// SWAP
	return prev;
}

/**
 * decrementDelta: will decrement every event's time that is found after
 * the clockEvent * parameter after by a given int
 * @time: the time to be decremented
 * @return: void
 *
 * this will keep the deltaClock synchronized when inserting events into the
 * middle of the clock.
 *
 */
void decrementDelta(int time, clockEvent * after) {
	// SWAP
	// check to see if deltaClock is NULL
	if (deltaClock == 0) {
		// SWAP
		return;
	}
	// SWAP

	// assign a pointer to the beginning of the linkedList
	clockEvent * current = deltaClock;

	// SWAP
	// advance the current pointer through the linked list until the
	// event after is found.
	while (current != after) {
		// SWAP
		if (current->next != NULL ) {
			// SWAP
			current = current->next;
			// SWAP
		} else
			// SWAP
			break;
	}

	// now that it's found, decrement the remaining events in the deltaClock.
	while (current->next != NULL ) {
		// SWAP
		current = current->next;
		// SWAP
		current->time = current->time - time;
		// SWAP

		// don't go below zero.
		if (current->time < 0) {
			// SWAP
			current->time = 0;
			// SWAP
		}
		// SWAP
		break;
	}
	// SWAP
	return;
}

/**
 * insertDeltaClock: insert a semaphore into the delta clock
 * @time: the delay in tenths of a second
 * @sem: the semaphore to be signaled at the end of the delay
 * @periodic: if the delay/signal should be repeated
 * @return: 0 if successful, -1 if failed
 *
 * This function should insert a clock event into the delta clock
 * in the appropriate place.  If the time is less than 0 or if the
 * semaphore is invalid, the function should return -1.  If this time
 * is 0, the semaphore should be signaled before returning.
 */
int insertDeltaClock(int time, Semaphore* semId, int periodic) {
	// SWAP
	semWait(globalDelta);
	// SWAP
	printf("INSERTING time = %d, semId = %d, periodic = %d\n", time, semId, periodic);
	// SWAP
	// check to see if time = 0, if so then if the event isn't periodic, signal
	// the semaphore (if it exists) and return.
	if (time == 0) {
		// SWAP
		if (periodic) {
			// SWAP
			return -1;
		}
		// SWAP
		if (!semId) {
			// SWAP
			return -1;
		}
		// SWAP
		semSignal(semId);
		// SWAP
		return 0;
	}

	// SWAP
	// check to see if the deltaClock has nothing in it.
	if (deltaClock == 0) {
		// SWAP
		// if the delta clock has nothing in it, then create an event and
		// assign it to the linkedList.
		deltaClock = (clockEvent*) malloc(sizeof(clockEvent));
		// SWAP
		deltaClock->init_value = time;
		// SWAP
		deltaClock->next = NULL;
		// SWAP
		deltaClock->periodic = periodic;
		// SWAP
		deltaClock->sem = semId;
		// SWAP
		deltaClock->time = time;
		// SWAP
	} else {
		// SWAP
		//check to see if the event should be placed in the very front
		if (time < deltaClock->time) {
			// SWAP
			// place the new event before all other events by making a copy of
			// the first event
			// then assigning deltaClock to be the event to be inserted,
			// then set the deltaClock's next to the copy.
			clockEvent *tmp = copyClock(deltaClock);
			// SWAP
			deltaClock->init_value = time;
			// SWAP
			deltaClock->next = tmp;
			// SWAP
			deltaClock->periodic = periodic;
			// SWAP
			deltaClock->sem = semId;
			// SWAP
			deltaClock->time = time;
			// SWAP

			// decrement the rest of the clock events by time so that the
			// inserted event doesn't mess up the other times. (SYNCRONIZATION)
			decrementDelta(time, deltaClock);
			// SWAP
		} else {
			// SWAP
			// if not to be inserted at the front, then it must be placed
			// in the middle somewhere.  To do this, find where it needs to be
			// placed, then insert into the linkedList, but subtract the times
			// of all events it is after to it's time, then subtract the times
			// of all events that are after the event by time.

			clockEvent *tmp = (clockEvent*) malloc(sizeof(clockEvent));
			// SWAP
			tmp->init_value = time;
			// SWAP
			tmp->next = NULL;
			// SWAP
			tmp->periodic = periodic;
			// SWAP
			tmp->sem = semId;
			// SWAP
			tmp->time = time;
			// SWAP

			//find the clockEvent that the event will be placed after
			clockEvent * toBeAddedAfter = findEventBefore(tmp);
			// SWAP

			// insert into the linked list
			if (toBeAddedAfter->next != NULL ) {
				// SWAP
				clockEvent * toBeAddedBefore = copyClock(toBeAddedAfter->next);
				// SWAP
				tmp->next = toBeAddedBefore;
				// SWAP
			}
			// SWAP
			toBeAddedAfter->next = tmp;
			// SWAP

			// decrement events after the added one to synchronize everything.
			decrementDelta(tmp->time, tmp);
			// SWAP
		}
	}
	// SWAP
	listDeltaClock();
	// SWAP
	semSignal(globalDelta);
	// SWAP
	return 0;
}

/**
 * deleteClockEvent: remove clock events from the delta clock
 * @sem: the semaphore whose clock events should be deleted
 * @return: 1 if successful, 0 if failed
 *
 * This function should remove all clock events from the delta clock
 * associated with the sempahore identified with semId. The delta
 * clock should be traversed to find all clock events associated
 * with the matching semaphore.  The events should be removed from the
 * linked list properly and any allocated memory should be freed.
 */
int deleteClockEvent(Semaphore * semId) {
	// check to see if deltaClock isn't NULL
	if (!deltaClock) {
		return 0;
	}
	// define pointers to keep track of clockEvents while traversing.
	clockEvent * current = deltaClock;
	clockEvent * prev = current;
	// go through the LinkedList by checking if the current pointer
	// has a next. (note - I will still have to check the last event since
	// it's next should be NULL.)
	while (current->next) {
		// if the current event matches the sem to be deleted
		if (current->sem == semId) {
			// keep track of current's next so that we can rearrange the linked
			// list correctly after the delete.
			clockEvent * next = current->next;
			// the case where the first on the list is to be deleted.
			if (current == prev) {
				current->next = NULL;
				free(current);
				deltaClock = next;

				// delete something in the middle.
			} else {
				prev->next = next;
				free(current);
			}
			return 1;
		}

		// advance pointers
		prev = current;
		current = current->next;
	}

	// check last one.
	if (current->sem == semId) {
		// check to see if prev == current.  This will only happen if there is
		// only one thing in the entire LinkedList.
		if (prev == current) {
			free(current);
			deltaClock = 0;
		} else {
			prev->next = NULL;
			free(current);
		}
		return 1;
	}
	return 0;
}

/**
 * tickDeltaClock: tick delta clock and signal any finished events
 *
 * Tick the delta clock on a 1/10th second resolution signaling events
 * as appropriate.  The clock should maintain a notion of time of last
 * call to manage clock drift as it is only called in pollInterrupts.
 * As semaphores are signaled, if the event is periodic, it should be
 * added back to the delta clock.
 */

int tickDeltaClock() {
	assert("Error: Trying to call _tickDeltaClock in User Mode\n" && superMode);

//	if (deltaClock)
//		listDeltaClock();

	// if deltaClock has somehting in it, decrement the first event's time
	if (deltaClock) {
		deltaClock->time = deltaClock->time - 1;

		// check for a value of zero in the first time of the deltaClock's
		// linked list and keep repeating this check until all ready
		// events have been signaled.
		while (deltaClock->time <= 0) {

			// signal the ready event.
			semSignal(deltaClock->sem);

			// delete the event from the LinkedList
			if (deleteClockEvent(deltaClock->sem) == 0)
				printf("**ERROR** Failed to delete a deltaClock event when it's time was up.\n");

			// check to see if deltaClock isn't empty before the next
			// while(deltaClock->time <=0) check and break the while loop
			// if it is empty to avoid segfaulting.
			if (!deltaClock)
				break;
		}
	}

	return 0;
}

/**
 *
 */
int testDeltaClock(int argc, char* argv[]) {
	// Implement a routine to test the delta clock
	printf("Testing Delta Clock\n");

	static char* deltaClockTestTaskArgv[] = { "deltaClockTestTask" };

	// Store Random Number generator
	srand((unsigned int) time(NULL ));

	// Create 10 Delta Clock Test Tasks
	int i;
	for (i = 0; i < 20; i++) {
		createTask("DeltaClockTest", deltaClockTestTask, MED_PRIORITY, 1, deltaClockTestTaskArgv);

	}

	return 0;
}

/**
 *
 */
int deltaClockTestTask(int argc, char* argv[]) {
	// Generate a random number for time
	int eventTime = (rand() % 50) * 5 + 1;

	struct timespec tstart = { 0, 0 }, tend = { 0, 0 };
	clock_gettime(CLOCK_MONOTONIC, &tstart);

	// Get taskId of this task
	int taskId = curTask;
	char * name = (char *) malloc(50);
	sprintf(name, "deltaClockTestTaskSem%d - %d", taskId, eventTime);
	Semaphore *semId = createSemaphore(name, BINARY, 0);

	if (TRUE)
		printf("Inserting Task %d into delta clock with time=%d\n", taskId, eventTime);

	// Create a Clock Event Blocking on this task's semaphore
	if (insertDeltaClock(eventTime, semId, 0) < 0) {
		printf("There was an error trying to insert Semaphore %s with time %d\n", semId->name,
				eventTime);
	}

	// Wait
	semWait(semId);

	clock_gettime(CLOCK_MONOTONIC, &tend);
	double difference = ((double) tend.tv_sec + 1.0e-9 * tend.tv_nsec)
			- ((double) tstart.tv_sec + 1.0e-9 * tstart.tv_nsec);

	printf("Task %d end. total time = %.5f, arg = %d ; diff = %.3f.\n", taskId, difference,
			eventTime, difference - ((double)eventTime / 10));
	free(name);
	return 0;
}
