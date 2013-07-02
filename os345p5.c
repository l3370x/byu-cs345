// os345p5.c - Scheduler
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

#define NUM_GROUPS	4
#define NUM_GROUP_REPORT_SECONDS	5

// ***********************************************************************
// project 5 variables

int P5_group(int argc, char* argv[]);
int groupReportTask(int argc, char* argv[]);
int groupTask(int argc, char* argv[]);

long int group_count[NUM_GROUPS];	// group counters
extern Semaphore* tics1sec;			// 1 second semaphore

// ***********************************************************************
// ***********************************************************************
// project5 command
//
//
int P5_project5(int argc, char* argv[])		// project 5
{
	int i;
	char** args[10];						// maximum of 10 tasks
	char arg1[16];
	char arg2[16];
	char arg3[16];

	static char* g1Argv[] = {"group1", "1", "1"};
	static char* g2Argv[] = {"group2", "2", "5"};
	static char* g3Argv[] = {"group3", "3", "10"};
	static char* g4Argv[] = {"group4", "4", "20"};
	static char* groupReportArgv[] = {"groupReport", "3"};

	printf("\nStarting Project 5");

	for (i=0; i<NUM_GROUPS; i++)
	{
		group_count[i] = 0;
		sprintf(arg1, "group%d", i + 1);
		sprintf(arg2, "%d", i + 1);
		sprintf(arg3, "%d", 1 + 4 * i);
		args[0] = arg1;
		args[1] = arg2;
		args[2] = arg3;

		printf("\narg1 = %s", arg1);
		printf("\narg2 = %s", arg2);
		printf("\narg3 = %s", arg3);

		createTask(arg1, P5_group, MED_PRIORITY, 3, args);
/*

	createTask("group1",					// task name
				P5_group,					// task
				MED_PRIORITY,				// task priority
				3,							// task argc
				g1Argv);					// task argument pointers

	createTask("group2",					// task name
				P5_group,					// task
				MED_PRIORITY,				// task priority
				3,							// task argc
				g2Argv);					// task argument pointers

	createTask("group3",					// task name
				P5_group,					// task
				MED_PRIORITY,				// task priority
				3,							// task argc
				g3Argv);					// task argument pointers

	createTask("group4",					// task name
				P5_group,					// task
				MED_PRIORITY,				// task priority
				3,							// task argc
				g4Argv);					// task argument pointers
*/
	}
	createTask("Group Report"	,			// task name
				groupReportTask,			// task
				MED_PRIORITY,				// task priority
				2,							// task argc
				groupReportArgv);			// task argument pointers
	return 0;
} // end P5_project5



// ***********************************************************************
// ***********************************************************************
// Group Report task
// ***********************************************************************
int groupReportTask(int argc, char* argv[])
{
	int i;
	int count = NUM_GROUP_REPORT_SECONDS;

	while (1)
	{
		while (count-- > 0)
		{
		   	// update every second
			SEM_WAIT(tics1sec);

		}
		printf("\nGroups:");
		for (i=0; i<NUM_GROUPS; i++) printf("%10ld", group_count[i]);

		count = NUM_GROUP_REPORT_SECONDS;
	}
	return 0;
} // end



// ***********************************************************************
//	group parent - create children
//		argv[0] = group base name
//		argv[1] = parent #
//		argv[2] = # of children
// ***********************************************************************
int P5_group(int argc, char* argv[])		// group 1
{
	int i, num_tasks;
	char buffer[32];

	num_tasks = atoi(argv[2]);

	for (i = 0; i < num_tasks; i++)
	{
		sprintf(buffer, "%s%c", argv[0], 'A'+i);
		createTask(buffer,					// task name
				groupTask,					// task
				MED_PRIORITY,				// task priority
				3,							// task argc
				argv);						// task argument pointers
	}
	return 0;
} // end P5_group



// ***********************************************************************
//	group task
//		argv[0] = group base name
//		argv[1] = parent #
//		argv[2] = # of children
// ***********************************************************************
int groupTask(int argc, char* argv[])		// group Task
{
	int group = atoi(argv[1]) - 1;

	while (1)
	{
		group_count[group]++;
		SWAP;
	}
	return 0;
} // end groupTask
