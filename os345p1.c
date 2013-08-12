// os345p1.c - Command Line Processor
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
#include <sys/ioctl.h>
#include <unistd.h>
#include "os345.h"

// The 'reset_context' comes from 'main' in os345.c.  Proper shut-down
// procedure is to long jump to the 'reset_context' passing in the
// power down code from 'os345.h' that indicates the desired behavior.

extern jmp_buf reset_context;
// -----

#define NUM_COMMANDS 52
typedef struct								// command struct
{
	char* command;
	char* shortcut;
	int (*func)(int, char**);
	char* description;
} Command;

// ***********************************************************************
// project 1 variables
//
extern long swapCount;					// number of scheduler cycles
extern char inBuffer[];					// character input buffer
extern Semaphore* inBufferReady;		// input buffer ready semaphore
extern bool diskMounted;				// disk has been mounted
extern char dirPath[];					// directory path
extern int curTask;						// current task
Command** commands;						// shell commands
extern char* myTime(char*);

// ***********************************************************************
// project 1 prototypes
Command** P1_init(void);
Command* newCommand(char*, char*, int (*func)(int, char**), char*);
void mySIGINTHandler(void);
void mySIGTSTPHandler(void);
void mySIGCONTHandler(void);
void mySIGTERMHandler(void);

extern void initializeHistory();
extern void saveCommandInHistory(char * command);
extern void freeHistory();
extern struct winsize w;

int is_empty(const char *s) {
	while (*s != '\0') {
		if (!isspace(*s))
			return 0;
		s++;
	}
	return 1;
}

// ***********************************************************************
// myShell - command line interpreter
//
// Project 1 - implement a Shell (CLI) that:
//
// 1. Prompts the user for a command line.
// 2. WAIT's until a user line has been entered.
// 3. Parses the global char array inBuffer.
// 4. Creates new argc, argv variables using malloc.
// 5. Searches a command list for valid OS commands.
// 6. If found, perform a function variable call passing argc/argv variables.
// 7. Supports background execution of non-intrinsic commands.
//
int P1_shellTask(int argc, char* argv[]) {
	int i, found, newArgc;					// # of arguments
	char** newArgv;							// pointers to arguments
	bool newTask = FALSE;					// bool if & found at end

	// initialize shell commands
	commands = P1_init();					// init shell commands
	initializeHistory();					// init history

	sigAction(mySIGINTHandler, mySIGINT);
	sigAction(mySIGTSTPHandler, mySIGTSTP);
	sigAction(mySIGCONTHandler, mySIGCONT);
	sigAction(mySIGTERMHandler, mySIGTERM);

	while (1) {

		// output prompt
		if (diskMounted)
			printf("\n%s>>", dirPath);
		else
			printf("\n%ld>>", swapCount);

		SEM_WAIT(inBufferReady);
		// wait for input buffer semaphore
		if (!inBuffer[0])
			continue;			// ignore blank lines
		// printf("%s", inBuffer);
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);	// get window size
		// printf ("lines %d\n", w.ws_row);
		// printf ("columns %d\n", w.ws_col);

		saveCommandInHistory(inBuffer);		// save command history

		SWAP
		// do context switch

		{
			// ?? >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			// ?? parse command line into argc, argv[] variables
			// ?? must use malloc for argv storage!
			static char *sp, *myArgv[MAX_ARGS];

			// init arguments
			newArgc = 1;
			myArgv[0] = sp = inBuffer;				// point to input string
			for (i = 1; i < MAX_ARGS; i++)
				myArgv[i] = 0;

			// parse input string
			while ((sp = strchr(sp, ' '))) {
				if (isspace(sp[1])) {
					*sp++ = 0;
					continue;
				}
				*sp++ = 0;
				myArgv[newArgc++] = sp;
				if (sp[0] == '"') {
					char * quoteCopy2 = strchr((sp + 1), '"');
					if (quoteCopy2 == NULL )		// second quote wasn't found
						break;
					sp = quoteCopy2 + 1;
					//*sp++ = 0;
					//myArgv[newArgc++] = sp;
				}
			}
			SWAP
			// remove whitespace args
			int i;
			for (i = 0; i < newArgc; i++) {
				if (is_empty(myArgv[i])) {
					int j;
					for (j = i; j < newArgc; j++) {
						int i;
						for (i = 0; i <= strlen(myArgv[j]); i++)
							myArgv[j][i] = 0;
						if (j != (newArgc - 1))
							strcpy(myArgv[j], myArgv[j + 1]);
					}
					i = i - 1;
					newArgc = newArgc - 1;
				}
			}
			SWAP
			// if blank after clearing whitespace, ignore line
			if (is_empty(myArgv[0]))
				continue;
			// check for trailing ampersand
			if (strchr(myArgv[newArgc - 1], '&') != NULL
					&& strlen(strchr(myArgv[newArgc - 1], '&')) == 1) {
				if (strlen(myArgv[newArgc - 1]) != 1) {
					// remove trailing ampersand if on last arg
					myArgv[newArgc - 1][strlen(myArgv[newArgc - 1]) - 1] = 0;
				} else {
					// remove trailing ampersand if is last arg
					newArgc = newArgc - 1;
				}
				newTask = TRUE;
			} else
				newTask = FALSE;
			SWAP
			// malloc argv stuff.
			newArgv = (char**) malloc(sizeof(char*) * newArgc);
			int z;
			for (z = 0; z < newArgc; z++) {
				newArgv[z] = malloc((strlen(myArgv[z]) + 1) * sizeof(char));
				strcpy(newArgv[z], myArgv[z]);
				// printf("newArgv[%d] = '%s'\n", z, newArgv[z]);
			}
		}	// ?? >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
		SWAP
		// look for command
		for (found = i = 0; i < NUM_COMMANDS; i++) {
			char newArgvInsensitive[sizeof(newArgv[0]) / sizeof(char)];
			strcpy(newArgvInsensitive, newArgv[0]);
			int j;
			for (j = 0; newArgvInsensitive[j]; j++) {
				newArgvInsensitive[j] = tolower(newArgvInsensitive[j]);
			}
			if (!strcmp(newArgvInsensitive, commands[i]->command)
					|| !strcmp(newArgvInsensitive, commands[i]->shortcut)) {
				// command found
				if (newTask) {
					createTask(&(*commands[i]->description), &(*commands[i]->func), MED_PRIORITY,
							newArgc, newArgv);
				} else {
					int retValue = (*commands[i]->func)(newArgc, newArgv);
					if (retValue)
						printf("\nCommand Error %d", retValue);
				}
				found = TRUE;
				break;
			}
		}
		if (!found)
			printf("\nInvalid command %s.", newArgv[0]);
		SWAP
		// ?? free up any malloc'd argv parameters
		int z;
		for (z = 0; z < newArgc; z++) {
			free(newArgv[z]);
		}
		free(newArgv);
		for (i = 0; i < INBUF_SIZE; i++)
			inBuffer[i] = 0;
	}
	freeHistory();
	return 0;						// terminate task
} // end P1_shellTask

void mySIGINTHandler(void) {
	sigSignal(-1, mySIGTERM);
	return;
}

void mySIGTSTPHandler(void) {
	sigSignal(-1, mySIGSTOP);
	return;
}

void mySIGCONTHandler(void) {
	return;
}

void mySIGTERMHandler(void) {
	killTask(curTask);
}

// ***********************************************************************
// ***********************************************************************
// P1 Project
//
int P1_project1(int argc, char* argv[]) {
	SWAP
	// do context switch

	return 0;
} // end P1_project1

// ***********************************************************************
// ***********************************************************************
// quit command
//
int P1_quit(int argc, char* argv[]) {
	int i;

	// free P1 commands
	for (i = 0; i < NUM_COMMANDS; i++) {
		free(commands[i]->command);
		free(commands[i]->shortcut);
		free(commands[i]->description);
		free(commands[i]);
	}
	free(commands);

	// powerdown OS345
	longjmp(reset_context, POWER_DOWN_QUIT);
	return 0;
} // end P1_quit

// **************************************************************************
// **************************************************************************
// lc3 command
//
int P1_lc3(int argc, char* argv[]) {
	strcpy(argv[0], "0");
	return lc3Task(argc, argv);
} // end P1_lc3

// **************************************************************************
// **************************************************************************
// add command
//
int P1_add(int argc, char* argv[]) {
	if (argc <= 1) {
		printf("\nUsage: add NUMBERS...\nAdd together all NUMBER(s)"
				" in decimal or hex format.");
		return 0;
	}
	SWAP
	int i;
	int answer = 0;
	for (i = 1; i < argc; i++) {
		int toAdd = 0;
		if (argv[i][0] == '0' && argv[i][1] == 'x') {
			// hex
			toAdd = (int) strtol(argv[i], NULL, 0);
		} else {
			// decimal
			toAdd = atoi(argv[i]);
		}
		answer += toAdd;
	}
	printf("\n\tAnswer = %d", answer);
	return 0;
} // end P1_add

// **************************************************************************
// **************************************************************************
// args command
//
int P1_args(int argc, char* argv[]) {
	if (argc <= 1) {
		printf("\nUsage: args ARUMENTS...\nLists argument(s).");
		return 0;
	}
	SWAP
	int i;
	for (i = 1; i < argc; i++) {
		printf("\n\tArgument [%d] - %s", i, argv[i]);
	}
	return 0;
} // end P1_args

// **************************************************************************
// **************************************************************************
// date_time command
//
int P1_date_time(int argc, char* argv[]) {
	if (argc != 1) {
		if (argv[0][0] == 'd')
			printf("\nUsage: date\nOutput current system date and time.");
		else
			printf("\nUsage: time\nOutput current system date and time.");
		return 0;
	}
	SWAP
	char * time = malloc(sizeof(char) * INBUF_SIZE);
	myTime(time);
	printf("\n\tThe current time is: %s", time);
	free(time);
	return 0;
} // end P1_date_time

// ***********************************************************************
// ***********************************************************************
// help command
//
int P1_help(int argc, char* argv[]) {
	int i;
	if (argc == 1) {
		// list commands
		for (i = 0; i < NUM_COMMANDS; i++) {
			SWAP
			// do context switch
			if (strstr(commands[i]->description, ":"))
				printf("\n%s", commands[i]->description);
			else
				printf("\n\t%4s: %s", commands[i]->shortcut, commands[i]->description);
		}
	} else if (argc == 2) {
		int proj = atoi(argv[1]);
		if (proj <= 0 || proj >= 7) {
			printf("\nError, project not found.");
			printf(
					"\nUsage: help [PROJECT]\nList help topics for a projct (default is all projects).");
			return 0;
		}
		char * theProj = malloc(sizeof(char) * 5);
		sprintf(theProj, "P%d:", proj);
		char * theProjEnd = malloc(sizeof(char) * 5);
		sprintf(theProjEnd, "P%d:", proj + 1);
		bool started = FALSE;
		for (i = 0; i < NUM_COMMANDS; i++) {
			SWAP
			// do context switch
			if (!started && !strstr(commands[i]->description, theProj)) {
				continue;
			}
			started = TRUE;
			if (strstr(commands[i]->description, theProjEnd)) {
				break;
			}
			if (strstr(commands[i]->description, theProj)) {
				printf("\n%s", commands[i]->description);
			} else
				printf("\n\t%4s: %s", commands[i]->shortcut, commands[i]->description);
		}
		free(theProj);
	} else {
		printf(
				"\nUsage: help [PROJECT]\nList help topics for a projct (default is all projects).");
	}
	return 0;
} // end P1_help

// ***********************************************************************
// ***********************************************************************
// initialize shell commands
//
Command* newCommand(char* command, char* shortcut, int (*func)(int, char**), char* description) {
	Command* cmd = (Command*) malloc(sizeof(Command));

	// get long command
	cmd->command = (char*) malloc(strlen(command) + 1);
	strcpy(cmd->command, command);

	// get shortcut command
	cmd->shortcut = (char*) malloc(strlen(shortcut) + 1);
	strcpy(cmd->shortcut, shortcut);

	// get function pointer
	cmd->func = func;

	// get description
	cmd->description = (char*) malloc(strlen(description) + 1);
	strcpy(cmd->description, description);

	return cmd;
} // end newCommand

Command** P1_init() {
	int i = 0;
	Command** commands = (Command**) malloc(sizeof(Command*) * NUM_COMMANDS);

	// system
	commands[i++] = newCommand("quit", "q", P1_quit, "Quit");
	commands[i++] = newCommand("kill", "kt", P2_killTask, "Kill task");
	commands[i++] = newCommand("reset", "rs", P2_reset, "Reset system");

	// P1: Shell
	commands[i++] = newCommand("project1", "p1", P1_project1, "P1: Shell");
	commands[i++] = newCommand("help", "he", P1_help, "OS345 Help");
	commands[i++] = newCommand("lc3", "lc3", P1_lc3, "Execute LC3 program");
	commands[i++] = newCommand("add", "add", P1_add, "Add all arguments together");
	commands[i++] = newCommand("args", "ar", P1_args,
			"List all parameters on the command line, numbers or strings.");
	commands[i++] = newCommand("date", "time", P1_date_time,
			"Output current system date and time.");

	// P2: Tasking
	commands[i++] = newCommand("project2", "p2", P2_project2, "P2: Tasking");
	commands[i++] = newCommand("semaphores", "sem", P2_listSems, "List semaphores");
	commands[i++] = newCommand("tasks", "lt", P2_listTasks, "List tasks");
	commands[i++] = newCommand("signal1", "s1", P2_signal1, "Signal sem1 semaphore");
	commands[i++] = newCommand("signal2", "s2", P2_signal2, "Signal sem2 semaphore");

	// P3: Jurassic Park
	commands[i++] = newCommand("project3", "p3", P3_project3, "P3: Jurassic Park");
	commands[i++] = newCommand("deltaclock", "dc", P3_dc, "List deltaclock entries");

	// P4: Virtual Memory
	commands[i++] = newCommand("project4", "p4", P4_project4, "P4: Virtual Memory");
	commands[i++] = newCommand("frametable", "dft", P4_dumpFrameTable, "Dump bit frame table");
	commands[i++] = newCommand("initmemory", "im", P4_initMemory, "Initialize virtual memory");
	commands[i++] = newCommand("touch", "vma", P4_vmaccess, "Access LC-3 memory location");
	commands[i++] = newCommand("stats", "vms", P4_virtualMemStats, "Output virtual memory stats");
	commands[i++] = newCommand("crawler", "cra", P4_crawler, "Execute crawler.hex");
	commands[i++] = newCommand("memtest", "mem", P4_memtest, "Execute memtest.hex");

	commands[i++] = newCommand("frame", "dfm", P4_dumpFrame, "Dump LC-3 memory frame");
	commands[i++] = newCommand("memory", "dm", P4_dumpLC3Mem, "Dump LC-3 memory");
	commands[i++] = newCommand("page", "dp", P4_dumpPageMemory, "Dump swap page");
	commands[i++] = newCommand("virtual", "dvm", P4_dumpVirtualMem, "Dump virtual memory page");
	commands[i++] = newCommand("root", "rpt", P4_rootPageTable, "Display root page table");
	commands[i++] = newCommand("user", "upt", P4_userPageTable, "Display user page table");

	// P5: Scheduling
	commands[i++] = newCommand("project5", "p5", P5_project5, "P5: Scheduling");
	//	commands[i++] = newCommand("stress1", "t1", P5_stress1, "ATM stress test1");
	//	commands[i++] = newCommand("stress2", "t2", P5_stress2, "ATM stress test2");

	// P6: FAT
	commands[i++] = newCommand("project6", "p6", P6_project6, "P6: FAT");
	commands[i++] = newCommand("change", "cd", P6_cd, "Change directory");
	commands[i++] = newCommand("copy", "cf", P6_copy, "Copy file");
	commands[i++] = newCommand("define", "df", P6_define, "Define file");
	commands[i++] = newCommand("delete", "dl", P6_del, "Delete file");
	commands[i++] = newCommand("directory", "dir", P6_dir, "List current directory");
	commands[i++] = newCommand("mount", "md", P6_mount, "Mount disk");
	commands[i++] = newCommand("mkdir", "mk", P6_mkdir, "Create directory");
	commands[i++] = newCommand("run", "run", P6_run, "Execute LC-3 program");
	commands[i++] = newCommand("space", "sp", P6_space, "Space on disk");
	commands[i++] = newCommand("type", "ty", P6_type, "Type file");
	commands[i++] = newCommand("unmount", "um", P6_unmount, "Unmount disk");

	commands[i++] = newCommand("fat", "ft", P6_dfat, "Display fat table");
	commands[i++] = newCommand("fileslots", "fs", P6_fileSlots, "Display current open slots");
	commands[i++] = newCommand("sector", "ds", P6_dumpSector, "Display disk sector");
	commands[i++] = newCommand("chkdsk", "ck", P6_chkdsk, "Check disk");
	commands[i++] = newCommand("final", "ft", P6_finalTest, "Execute file test");

	commands[i++] = newCommand("open", "op", P6_open, "Open file test");
	commands[i++] = newCommand("read", "rd", P6_read, "Read file test");
	commands[i++] = newCommand("write", "wr", P6_write, "Write file test");
	commands[i++] = newCommand("seek", "sk", P6_seek, "Seek file test");
	commands[i++] = newCommand("close", "cl", P6_close, "Close file test");

	assert(i == NUM_COMMANDS);

	return commands;

} // end P1_init
