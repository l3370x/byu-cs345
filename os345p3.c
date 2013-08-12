// os345p3.c - Jurassic Park
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
#include <time.h>
#include <assert.h>
#include "os345.h"
#include "os345park.h"

// ***********************************************************************
// project 3 variables

// Jurassic Park
extern TCB tcb[MAX_TASKS];
extern JPARK myPark;
extern Semaphore* parkMutex;                                            // protect park access
extern Semaphore* fillSeat[NUM_CARS];                   // (signal) seat ready to fill
extern Semaphore* seatFilled[NUM_CARS];         // (wait) passenger seated
extern Semaphore* rideOver[NUM_CARS];                   // (signal) ride over
Semaphore* dcChange;
Semaphore* dcMutex;
Semaphore* waitCar;
Semaphore* seatTaken[NUM_CARS];
Semaphore* seatLeave[NUM_CARS];
Semaphore* sleepingDrivers;
Semaphore* needDriverMutex;
Semaphore* holdVisitor[NUM_VISITORS];
Semaphore* ticket;
Semaphore* needTicket;
Semaphore* needDrive;
Semaphore* readyToDrive;
Semaphore* buyTicket;
Semaphore* ticketReady;
Semaphore* museum;
Semaphore* gift;
Semaphore* giftMutex;
Semaphore* park;
extern int superMode;
Semaphore* finished;
// ***********************************************************************
// project 3 functions and tasks
void CL3_project3(int, char**);
void CL3_dc(int, char**);

Delta *clocks[1024];
int dCount = 0;
int currentCar;
int timelock = 0;
//************************
int init() {
	char buf[32];
	int i = 0;
	for (i = 0; i < 1024; i++) {
		clocks[i] = (Delta*) malloc(sizeof(Delta));
		clocks[i]->time = 0;
		clocks[i]->event = NULL;
	}

	dcChange = createSemaphore("dcChange", 0, 1);

	dcMutex = createSemaphore("dcMutex", 0, 1);

	waitCar = createSemaphore("waitCar", 0, 0);

	for (i = 0; i < NUM_CARS; i++) {
		sprintf(buf, "seatTaken%d", i);
		seatTaken[i] = createSemaphore(buf, 0, 0);
		sprintf(buf, "seatLeave%d", i);
		seatLeave[i] = createSemaphore(buf, 1, 0);
	}

	for (i = 0; i < NUM_VISITORS; i++) {
		sprintf(buf, "holdVisitor%d", i);
		holdVisitor[i] = createSemaphore(buf, 0, 0);
	}

	ticket = createSemaphore("ticket", 1, MAX_TICKETS);

	needTicket = createSemaphore("needTicket", 0, 0);

	ticketReady = createSemaphore("ticketReady", 0, 0);

	buyTicket = createSemaphore("buyTicket", 0, 0);

	museum = createSemaphore("museum", 1, MAX_IN_MUSEUM);

	gift = createSemaphore("gift", 1, MAX_IN_GIFTSHOP);

	giftMutex = createSemaphore("changeGift", 0, 1);

	park = createSemaphore("park", 1, MAX_IN_PARK);

	finished = createSemaphore("finished", 1, 0);

	needDriverMutex = createSemaphore("needDriverMutex", 0, 1);

	needDrive = createSemaphore("needDrive", 0, 0);

	readyToDrive = createSemaphore("readyToDrive", 0, 0);

	sleepingDrivers = createSemaphore("sleepingDrivers", 0, 0);
	return 0;
}
// ***********************************************************************
// ***********************************************************************
// project3 command
int P3_project3(int argc, char* argv[]) {
	char buf[32];
	char* newArgv[2];
	init();
	// start park
	sprintf(buf, "jurassicPark");
	newArgv[0] = buf;
	createTask(buf,                                // task name
			jurassicTask,                           // task
			MED_PRIORITY,                           // task priority
			1,                                                              // task count
			newArgv);                                       // task argument

	// wait for park to get initialized...
	while (!parkMutex)
		SWAP
	;
	printf("\nStart Jurassic Park...");

	//?? create car, driver, and visitor tasks here
	//create 4 cars.
	int i;
	for (i = 0; i < NUM_CARS; i++) {
		sprintf(buf, "carTask%d", i);
		char **temparg = (char**) malloc(sizeof(char**));
		*temparg = (char*) malloc(sizeof(char));
		sprintf(temparg[0], "%d", i);
		//printf("\nCreating Car%s",temparg[0]);
		createTask(buf, carTask, MED_PRIORITY, 1, temparg);

	}
	//create visitors:  45 as default

	for (i = 0; i < NUM_VISITORS; i++) {
		sprintf(buf, "visitorTask%d", i);
		char **temparg = (char**) malloc(sizeof(char*));
		*temparg = (char*) malloc(sizeof(char) + 1);
		sprintf(temparg[0], "%d", i);
		//printf("\nCreating Car%s",temparg[0]);
		createTask(buf, visitorTask, MED_PRIORITY, 1, temparg);

	}
	//create driver
	for (i = 0; i < NUM_DRIVERS; i++) {
		sprintf(buf, "driverTask%d", i);
		char **temparg = (char**) malloc(sizeof(char**));
		*temparg = (char*) malloc(sizeof(char));
		sprintf(temparg[0], "%d", i);
		//printf("\nCreating Car%s",temparg[0]);
		createTask(buf, driverTask, MED_PRIORITY, 1, temparg);

	}
	return 0;
} // end project3

// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[]) {
	printf("\nDelta Clock");
	// ?? Implement a routine to display the current delta clock contents
	int i;
	for (i = 0; i < dCount; i++) {
		printf("\nDelta Clock%d: time %d  Sem %s", i, clocks[i]->time, clocks[i]->event->name);
	}
	printf("\nTo Be Implemented!");
	return 0;
} // end CL3_dc

//******************************************
//insert delta closk
int insertDeltaClock(int time, Semaphore* sem) {
	//printf("\nnew delat1");
	if (!superMode)
		SWAP
	;
	timelock = 1;
	semWait(dcMutex);
	if (!superMode)
		SWAP
	;
	int i;
	for (i = 0; i <= dCount; i++) { //printf("\nnew delat2");

		if (clocks[i]->event == NULL ) //at end of the list
		{  //printf("\nnew delat3");
			dCount++;
			clocks[i]->time = time;
			clocks[i]->event = sem;
			if (!superMode)
				SWAP
			;
			break;
		} else if (clocks[i]->time <= time)  // iterate front elements
				{       //printf("\n delta2%d",time);
			time = time - clocks[i]->time;
			if (!superMode)
				SWAP
			;

		} else if (clocks[i]->time > time) {
			clocks[i]->time -= time;
			//create new clock
			Delta* temp = (Delta*) malloc(sizeof(Delta));
			if (!superMode)
				SWAP
			;
			temp->time = time;
			temp->event = sem;
			if (!superMode)
				SWAP
			;
			//insert new clock at position i
			int j;
			for (j = dCount; j > i; j--) {
				clocks[j] = clocks[j - 1];
			}
			clocks[i] = temp;
			dCount++;
			if (!superMode)
				SWAP
			;
			break;

		}
	}
	semSignal(dcMutex);
	timelock = 0;
	return 0;
}
//***********************************
//delete sem in delta clock

int deleteDeltaClock(Semaphore* sem) {

	semWait(dcMutex);
	int i;
	for (i = 0; i < dCount; i++) {
		if (clocks[i]->event != NULL && strcmp(clocks[i]->event->name, sem->name) == 0) {
			if (clocks[i + 1]->event != NULL ) {
				clocks[i + 1]->time += clocks[i]->time;
			}

			free(clocks[i]);
			int j;
			for (j = i; j < dCount; j++) {
				clocks[j] = clocks[j + 1];
			}

			dCount--;
			clocks[dCount] = (Delta*) malloc(sizeof(Delta));
			clocks[dCount]->time = 0;
			clocks[dCount]->event = NULL;
		}
	}

	semSignal(dcMutex);
}
//*******************************
// time-1 for top event. Signal it if time == 0
// this part called in pollinterupt, so it is in super mode(kernal)
//  1. could not swap here.
//  2. could not semwait here.
int dcRefresh() {
	if (!dcMutex)
		return 1;
	if (dcMutex->state == 0)
		return 0;
	if (timelock == 1)
		return 0;
	//printf("dcRefresh");
	//semWait(dcMutex);
	if (dCount > 0 && clocks[0]->event != NULL ) {
		if (clocks[0]->time > 0)        // first delta clock -1
				{
			clocks[0]->time--;

		}
		while (clocks[0]->time == 0 && clocks[0]->event != NULL ) //if first delta clock is 0, semsignal and remove
		{
			semSignal(clocks[0]->event);
			free(clocks[0]);
			int i;
			for (i = 0; i < dCount; i++) {
				clocks[i] = clocks[i + 1];
			}
			dCount--;
			clocks[dCount] = (Delta*) malloc(sizeof(Delta));
			if (!superMode)
				SWAP
			;
			clocks[dCount]->time = 0;
			clocks[dCount]->event = NULL;

		}
	}

	//semSignal(dcChange);
	return 1;

}
/*
 // ***********************************************************************
 // ***********************************************************************
 // ***********************************************************************
 // ***********************************************************************
 // ***********************************************************************
 // ***********************************************************************
 // delta clock command
 int P3_dc(int argc, char* argv[])
 {
 printf("\nDelta Clock");
 // ?? Implement a routine to display the current delta clock contents
 //printf("\nTo Be Implemented!");
 int i;
 for (i=0; i<numDeltaClock; i++)
 {
 printf("\n%4d%4d  %-20s", i, deltaClock[i].time, deltaClock[i].sem->name);
 }
 return 0;
 } // end CL3_dc

 */
// ***********************************************************************
// display all pending events in the delta clock list
void printDeltaClock(void) {
	int i;
	SWAP
	for (i = 0; i < dCount; i++) {
		SWAP
		printf("\n%4d%4d  %-20s", i, clocks[i]->time, clocks[i]->event->name);
	}
	SWAP
	return;
}

int timeTaskId;
// ***********************************************************************
// test delta clock
int P3_tdc(int argc, char* argv[]) {
	init();
	createTask("DC Test",                  // task name
			dcMonitorTask,          // task
			10,                                     // task priority
			argc,                                   // task arguments
			argv);

	timeTaskId = createTask("Time",         // task name
			timeTask,       // task
			10,                     // task priority
			argc,                   // task arguments
			argv);
	return 0;
} // end P3_tdc

// ***********************************************************************
// monitor the delta clock task
int dcMonitorTask(int argc, char* argv[]) {
	int i, flg;
	char buf[32];
	Semaphore *event[10];
	// create some test times for event[0-9]
	int ttime[10] = { 90, 300, 50, 170, 340, 300, 50, 300, 40, 110 };

	for (i = 0; i < 10; i++) {
		sprintf(buf, "event[%d]", i);
		event[i] = createSemaphore(buf, BINARY, 0);
		printf("\n dc monitor");
		insertDeltaClock(ttime[i], event[i]);
	}
	printDeltaClock();
	deleteDeltaClock(event[6]);
	insertDeltaClock(50, event[6]);

	printDeltaClock();
	while (dCount > 0) {
		//SEM_WAIT(dcChange)
		flg = 0;
		for (i = 0; i < 10; i++) {
			if (event[i]->state == 1) {
				printf("\n  event[%d] signaled", i);
				event[i]->state = 0;
				flg = 1;
			}
		}
		if (flg)
			printDeltaClock();
	}
	printf("\nNo more events in Delta Clock");

	// kill dcMonitorTask
	tcb[timeTaskId].state = S_EXIT;
	return 0;
} // end dcMonitorTask

extern Semaphore* tics1sec;

// ********************************************************************************************
// display time every tics1sec
int timeTask(int argc, char* argv[]) {
	char svtime[64];                                                // ascii current time
	while (1) {
		SEM_WAIT(tics1sec)
		printf("\nTime = %s  clocks=%d", myTime(svtime), dCount);
	}
	return 0;
} // end timeTask

//****************************************************************************************
//****************************************************************************************
//****************************************************************************************
//****************************************************************************************
//****************************************************************************************
// car task
int carTask(int argc, char* argv[]) {
	//printf("\ncarTask%d%s",argc,argv[0]);
	int carId;
	SWAP
	int visitor[2];
	SWAP
	if (argc == 1) {       //printf("\ncarId " );
		SWAP
		carId = atoi(argv[0]);
		SWAP
		//printf("\ncarId:%d",carId);
	} else {
		SWAP
		return -1;
	}
	SWAP
	;
	while (1) {
		//printf("\nCar:%d",carId);

		SWAP
		;
		//get 2 tourist
		int i;
		for (i = 0; i < 2; i++) {
			semWait(fillSeat[carId]);
			SWAP
			;
			currentCar = carId;
			semSignal(waitCar);
			SWAP
			;
			// visitor wait on seatTaken[carId]
			semSignal(seatFilled[carId]);
			SWAP
			;
			//printf("\nCar:%d %dseat",carId,i+1);
		}

		//try to get 1 driver
		semWait(fillSeat[carId]);
		SWAP
		currentCar = carId;
		semSignal(waitCar);
		SWAP
		semWait(needDriverMutex);
		SWAP
		semSignal(needDrive);
		SWAP
		semSignal(sleepingDrivers);
		SWAP
		currentCar = carId;
		semWait(readyToDrive);
		SWAP
		semSignal(seatFilled[carId]);
		SWAP
		//printf("\nCar:%d 3seat",carId);
		semSignal(needDriverMutex);
		SWAP

		//riding
		semWait(rideOver[carId]);
		SWAP
		//dump visitor
		for (i = 0; i <= NUM_SEATS; i++) {
			//printf("\nunload visitor+driver");
			semSignal(seatTaken[carId]);
			SWAP
		}

		//printf("\nCar:%d rideover",carId);
	}
	SWAP
	//dump driver

	//continue
	//semSignal(pausePark);

	return 0;
}

//****************************************************************************************
//****************************************************************************************
//****************************************************************************************
//****************************************************************************************
//****************************************************************************************
// visitor task
int visitorTask(int argc, char* argv[]) {

	int visitorId;
	SWAP
	if (argc == 1) {       //printf("\ncarId " );
		SWAP
		visitorId = atoi(argv[0]);
		SWAP
		//printf("\ncarId:%d",carId);
	} else {
		SWAP
		return -1;
	}
	SWAP
	int arriveTime = rand() % 10 * 10;
	SWAP
	int tickettime = rand() % 3 * 10;
	SWAP
	int museumtime = rand() % 3 * 10;
	SWAP
	int gifttime = rand() % 3 * 10;
	//arrive outside park
	SWAP
	semWait(parkMutex);
	SWAP
	myPark.numOutsidePark++;
	SWAP
	semSignal(parkMutex);
	SWAP
	insertDeltaClock(arriveTime, holdVisitor[visitorId]);
	SWAP
	semWait(holdVisitor[visitorId]);
	SWAP
	//printf("\nVisitor%d arrived",visitorId);

	//before buy ticket

	//enter park.
	semWait(park);       //20 limit
	SWAP
	semWait(parkMutex);
	SWAP
	myPark.numOutsidePark--;
	myPark.numInPark++;
	myPark.numInTicketLine++;
	semSignal(parkMutex);
	SWAP
	insertDeltaClock(tickettime, holdVisitor[visitorId]);
	SWAP
	semWait(holdVisitor[visitorId]);
	//printf("\nVisitor%d deside to buy ticket",visitorId);
	SWAP

	//buy ticket
	semWait(ticket);       //12limit
	SWAP
	semWait(needDriverMutex);
	SWAP
	semSignal(sleepingDrivers);
	SWAP
	semSignal(needTicket);
	SWAP
	semWait(ticketReady);
	SWAP
	semSignal(buyTicket);
	SWAP
	semSignal(needDriverMutex);
	SWAP
	semWait(parkMutex);
	SWAP
	myPark.numTicketsAvailable--;
	myPark.numInTicketLine--;
	myPark.numInMuseumLine++;
	SWAP
	semSignal(parkMutex);

	SWAP
	//to museum line
	insertDeltaClock(museumtime, holdVisitor[visitorId]);
	SWAP
	semWait(holdVisitor[visitorId]);
	SWAP
	//printf("\nVisitor%d in museum line",visitorId);
	//into museum
	semWait(museum);        //5 limit
	SWAP
	semWait(parkMutex);
	SWAP
	myPark.numInMuseumLine--;
	myPark.numInMuseum++;
	SWAP
	semSignal(parkMutex);
	SWAP
	insertDeltaClock(museumtime, holdVisitor[visitorId]);
	SWAP
	semWait(holdVisitor[visitorId]);
	SWAP
	//leave museum
	semWait(parkMutex);
	SWAP
	semSignal(museum);
	myPark.numInMuseum--;
	myPark.numInCarLine++;
	SWAP
	semSignal(parkMutex);
	semWait(waitCar);
	//enter car

	/*for(int i=0;i<NUM_CARS;i++)
	 {
	 if(myPark.cars[i].location==33)
	 {*/
	semWait(parkMutex);
	SWAP
	//printf("\nresell ticket.sem:%d available:%d",ticket->state,myPark.numTicketsAvailable);
	myPark.numInCarLine--;
	myPark.numInCars++;
	myPark.numTicketsAvailable++;
	//myPark.numRidesTaken++;
	semSignal(parkMutex);
	SWAP
	semSignal(ticket);
	//printf("\nresell ticket.sem:%d available:%d",ticket->state,myPark.numTicketsAvailable);
	SWAP
	semSignal(parkMutex);
	SWAP
	semWait(seatTaken[currentCar]);
	semWait(parkMutex);
	myPark.numInCars--;
	myPark.numInGiftLine++;
	semSignal(parkMutex);

	SWAP
	insertDeltaClock(gifttime, holdVisitor[visitorId]);
	SWAP
	semWait(holdVisitor[visitorId]);
	SWAP
	//semWait(giftMutex);
	semWait(gift);
	SWAP
	//inside gift
	semWait(parkMutex);
	SWAP
	myPark.numInGiftLine--;
	myPark.numInGiftShop++;
	SWAP
	semSignal(parkMutex);
	SWAP
	insertDeltaClock(gifttime, holdVisitor[visitorId]);
	SWAP

	//leave park
	semWait(holdVisitor[visitorId]);
	SWAP
	semWait(parkMutex);
	//printf("\ngift.sem:%d inside gift:%d",gift->state,myPark.numInGiftShop);
	SWAP
	myPark.numInGiftShop--;
	myPark.numInPark--;
	myPark.numExitedPark++;
	SWAP
	semSignal(gift);
	SWAP
	//printf("\ngift.sem:%d insidegift:%d",gift->state,myPark.numInGiftShop);
	semSignal(park);
	SWAP
//      printf("\nPark limit%d",park->state);
	semSignal(parkMutex);
	SWAP
	semWait(finished);
	return 0;
}
//****************************************************************************************
//****************************************************************************************
//****************************************************************************************
//****************************************************************************************
//****************************************************************************************
// driver task
int driverTask(int argc, char* argv[]) {

	int driverId;
	if (argc == 1) {       //printf("\ncarId " );
		driverId = atoi(argv[0]);
		//printf("\ncarId:%d",carId);
	} else {
		return -1;
	}

	while (1) {

		semWait(parkMutex);
		SWAP
		myPark.drivers[driverId] = 0;
		SWAP
		semSignal(parkMutex);
		SWAP
		semWait(sleepingDrivers);
		SWAP
		if (semTryLock(needTicket) == 1) {
			semWait(parkMutex);
			SWAP
			myPark.drivers[driverId] = -1;
			SWAP
			semSignal(parkMutex);
			SWAP
			semSignal(ticketReady);
			SWAP
			semWait(buyTicket);
			SWAP

		} else if (semTryLock(needDrive) == 1) {
			semSignal(readyToDrive);
			SWAP

			/*for(int i=0;i<NUM_CARS;i++)
			 {
			 if(myPark.cars[i].location==33)
			 {*/
			semWait(parkMutex);
			SWAP
			myPark.drivers[driverId] = currentCar + 1;
			SWAP
			semSignal(parkMutex);
			SWAP
			semWait(seatTaken[currentCar]);
			SWAP
			/*i=NUM_CARS;
			 }
			 }*/

		}

	}

	return 0;
}
