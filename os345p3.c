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

extern int curTask;
extern int insertDeltaClock(int i, Semaphore * s);
extern int deleteClockEvent(Semaphore * s);
extern void listDeltaClock();

int testDeltaClock(int argc, char* argv[]);
int deltaClockTestTask(int argc, char* argv[]);

// ***********************************************************************
// project 3 variables

// Jurassic Park
extern JPARK myPark;
extern Semaphore* parkMutex;					// protect park access
extern Semaphore* fillSeat[NUM_CARS];			// (signal) seat ready to fill
extern Semaphore* seatFilled[NUM_CARS];			// (wait) passenger seated
extern Semaphore* rideOver[NUM_CARS];			// (signal) ride over

// ***********************************************************************
// project 3 functions and tasks
void CL3_project3(int, char**);
void CL3_dc(int, char**);

int createDriverCarVisitorTasks(int argc, char* argv[]);
int visitorTask(int a, char * b[]);
int carTask(int a, char * b[]);
int driverTask(int a, char * b[]);
void changeParkGlobalInt(int * toChange, int changeBy);

// my globals
int RANDOM_UNSEEDED = 1;
#define NELEMS(x)  (sizeof(x) / sizeof(x[0]))
Semaphore * inThePark;   // counting semaphore that will control the number of poeple in the park.
Semaphore * ticket; // coutning semaphore that will control the number of tickets sold to visitors
Semaphore * museum;    // coutning semaphore that will control the number of people in the museum.
Semaphore * giftShop; // coutning semaphore that will control the number of people in the gift shop.

Semaphore * getPassenger;       // binary semaphore that will control the next person in a car.
Semaphore * seatTaken;   // binary semaphore that will control the when a person has taken a seat.
Semaphore * passengerSeated;  // binary semaphore that will control when a person is fully seated.
Semaphore * nextPassenger; // semaphore to be called once ride is done (this is passed from visitor to car.
Semaphore * seatLock;		// binary semaphore that will control the critical section of loading

Semaphore * needTicket;	// counting semaphore to count the nubmer of visitors that need aa ticket

int semGlobal;			// this is a global int that will keep all semaphore names different

Semaphore * driverSleep;        // semaphore to awaken drivers
Semaphore * driverLock;         // semaphore mutex to protect drivers
Semaphore * ticketDataReady;   // semaphore that will let driver tell visitor that ticket is ready
Semaphore * ticketGiven;	// binary semaphore that tells when driver has given visitor ticket

Semaphore * carDataReady;	// semaphore that will let driver tell car that bottom is ready to sit
Semaphore * carGiven;		// binary semaphore that tells when driver has given car their bottom
Semaphore * carReadyForDriver;// semaphore that will allow car to signal drivers taht one is ready

Semaphore * driverForCarSet;	// wait for driver to set state to car
Semaphore * driverForCarAck;	// acknowledge driver set state to car

int nextCarDriver;		// int to pass next driver to car
int carForDriver;		// int to pass which car to driver
Semaphore * carForDriverSem;	// semaphore for car to pass to driver that will signal rideOver


void initializeGlobalVariables() {
	// create MAX_IN_PARK semaphore
	inThePark = createSemaphore("in_the_park", COUNTING, MAX_IN_PARK);

	// create MAX_TICKETS tickets using counting semaphore
	ticket = createSemaphore("tickets_held", COUNTING, MAX_TICKETS);

	// create MAX_IN_MUSEUM semaphore
	museum = createSemaphore("in_the_museum", COUNTING, MAX_IN_MUSEUM);

	// create MAX_IN_GIFTSHOP semaphore
	giftShop = createSemaphore("in_the_giftShop", COUNTING, MAX_IN_GIFTSHOP);

	// create semaphores to control visitor and car interaction
	getPassenger = createSemaphore("getPassenger", BINARY, 0);
	seatTaken = createSemaphore("seatTaken", BINARY, 0);
	passengerSeated = createSemaphore("pasSeated", BINARY, 0);
	seatLock = createSemaphore("seatLock", BINARY, 1);

	// create semaphores to control driver and car interaction
	carGiven = createSemaphore("carGiven", BINARY, 0);
	carDataReady = createSemaphore("CarDatReady", BINARY, 0);
	carReadyForDriver = createSemaphore("carRdy4dvr", COUNTING, 0);
	driverForCarSet = createSemaphore("dvrNeedsCarName", BINARY, 0);
	driverForCarAck = createSemaphore("dvrAckCarState", BINARY, 0);

	// create semaphores for each driver
	driverSleep = createSemaphore("drvSleep", COUNTING, 0);
	driverLock = createSemaphore("dvrlock", BINARY, 1);
	ticketGiven = createSemaphore("ticketGiven", BINARY, 0);
	ticketDataReady = createSemaphore("ticDatReady", BINARY, 0);

	// create global int to maintain unique semaphores when waiting.
	semGlobal = 1;

}

// this function will return a random int that is between a requested
// min and max.  the returned int can include max and min.
//
// int min = lowest random number wanted
// int max = highest random number wanted
//
// return value = random int between min and max (endpoints included).
int getRandomBetween(int min, int max) {

	// seed rand() to the current time for pseudo random generator.
	// and only do this the first call to getRandBetween.
	if (RANDOM_UNSEEDED) {
		srand(time(NULL ));
		RANDOM_UNSEEDED = 0;
	}
	return rand() % (1 + max - min) + min;
}

// this function will safely access a global variable and increment it.
// it is used to tie a unique name to a semaphore.
int getSemGlobal() {
	// protect shared memory access
	semWait(parkMutex);
	int toReturn = semGlobal;

	// access inside park variables
	semGlobal = semGlobal + 1;

	// release protect shared memory access
	semSignal(parkMutex);
	return toReturn;
}

void changeParkGlobalInt(int * toChange, int changeBy) {
	// access inside park variables
	(*toChange) = (*toChange) + changeBy;
}

// this function will do a non-blocking wait for a given ammount of milliseconds
// it will create a unique semaphore based off the caller's argv info
//
// int milliseconds = number of milliseconds to wait
// char* argv[] = the caller's argv info.
void waitMilliseconds(int milliseconds, char* argv[]) {

	// set up variables for semaphore and create it.  To be sure to
	// find a name that isn't being used by any semaphore, create a name by using the calling functions
	// argv name and id and the current time and a global int that is safely incremented
	// on each call to this function.
	char name[128];
	sprintf(name, "jpWait%s%s%d", argv[0], argv[1], getSemGlobal());
	Semaphore * s;
	s = createSemaphore(name, BINARY, 0);
	assert(s);

	// convert milliseconds to tenth of seconds for delta clock
	int tenths = milliseconds / 100;

	// insert semaphore into delta clock for certain time.
	insertDeltaClock(tenths, s);

	// wait for the semaphore to end
	semWait(s);

	// clean up clock and sempaphre.
	deleteClockEvent(s);
	deleteSemaphore(&s);
}

// this function will wait a random number of seconds between a given min
// and maximum input.
// int min = minimum value of time to wait
// int max = maximum value of time to wait
// char* argv[] = the calling function's argv
//
// this will always assume seconds.
void waitRandSecBetween(int min, int max, char*argv[]) {

	assert("max is less than or equal to min in waitRandSecBetween!" && (max > min));

	// get a random number in the requested interval.
	int toWait = getRandomBetween(min, max);

	// Convert to milliseconds for the waitMilliseconds function.
	toWait = toWait * 1000;

	// wait the number of milliseconds.
	waitMilliseconds(toWait, argv);
}

// ***********************************************************************
// ***********************************************************************
// project3 command
int P3_project3(int argc, char* argv[]) {
	char buf[32];
	char* newArgv[2];

	initializeGlobalVariables();

	// start park
	sprintf(buf, "jurassicPark");
	newArgv[0] = buf;
	createTask(buf,				// task name
			jurassicTask,				// task
			MED_PRIORITY,				// task priority
			1,								// task count
			newArgv);					// task argument

	// wait for park to get initialized...
	while (!parkMutex)
		SWAP
	;
	printf("\nStart Jurassic Park...");

	// create car, driver, and visitor tasks here
	createDriverCarVisitorTasks(argc, argv);

	return 0;
} // end project3

int createDriverCarVisitorTasks(int argc, char* argv[]) {
	int i;
	char* carArgv[2];
	char nameID[32], carID[32], name[32];
	char* driverArgv[2];
	char driverNameID[32], driverID[32], driverName[32];
	char* visitorArgv[2];
	char vistorNameID[32], visitorID[32], visitorName[32];

	// Create driver tasks represented by NUM_DRIVERS
	sprintf(driverName, "driver");
	driverArgv[0] = driverName;
	driverArgv[1] = driverID;
	sprintf(driverArgv[0], "driverTask");
	for (i = 0; i < NUM_DRIVERS; i++) {

		sprintf(driverArgv[1], "%d", i);
		sprintf(driverNameID, "driverTask%d", i);
		if(createTask(driverNameID, driverTask, MED_PRIORITY, 2, driverArgv)){
			// yay
		}
	}

	// Create car tasks represented by NUM_CARS
	sprintf(name, "car");
	carArgv[0] = name;
	carArgv[1] = carID;
	sprintf(carArgv[0], "carTask");
	for (i = 0; i < NUM_CARS; i++) {
		sprintf(carArgv[1], "%d", i);
		sprintf(nameID, "carTask%d", i);
		if(createTask(nameID, carTask, MED_PRIORITY, 2, carArgv)){
			// yay;
		}
	}

	// Create car tasks represented by NUM_VISITORS of argv[2]
	sprintf(visitorName, "visitor");
	visitorArgv[0] = visitorName;
	visitorArgv[1] = visitorID;
	sprintf(visitorArgv[0], "visitorTask");
	for (i = 0; i < NUM_VISITORS; i++) {
		sprintf(visitorArgv[1], "%d", i);
		sprintf(vistorNameID, "visitorTask%d", i);

		if(createTask(vistorNameID, visitorTask, MED_PRIORITY, 2, visitorArgv)){
			// yay;
			printf("yay");
		}
	}

	return 0;
}

// ***********************************************************************
// ***********************************************************************
// carTask
int carTask(int argc, char* argv[]) {

	Semaphore * riders[NUM_SEATS];
	int carID = atoi(argv[1]);
	int carDriver = 0;
	int seat = 0;
	Semaphore * driverRideOverSem;
	// while there are still people in the park
	while (myPark.numExitedPark < NUM_VISITORS) {
		swapTask();

		// wait for fillSeat (in loading position and someone in car line)
		semWait(fillSeat[carID]);

		// tell visitors that seat is ready to be filled
		semSignal(getPassenger);

		// wait for visitor to claim seat
		semWait(seatTaken);

		// this will hold the semaphore of the rider that needs to be signaled when rideOver.
		riders[seat] = nextPassenger;
		seat = seat + 1;

		// tell visitor that he is fully seated.
		semSignal(passengerSeated);

		// if car full, get a driver
		if ((seat == NUM_SEATS) && (carDriver == 0)) {
			// get driver
			// first wake up driver
			semSignal(driverSleep);
			semSignal(carReadyForDriver);

			// wait for carDataReady
			semWait(carDataReady);
			carDriver = nextCarDriver;
			carForDriver = carID;

			// create semaphore for driver Riding
			char name[30];
			sprintf(name, "driverRiding%d%d", carDriver, carID);
			driverRideOverSem = createSemaphore(name, BINARY, 0);
			carForDriverSem = driverRideOverSem;

			// tell driver that seat is acknowledged
			semSignal(carGiven);
			semWait(driverForCarSet);
			semSignal(driverForCarAck);
		}


		semSignal(seatFilled[carID]);
		// if car full, drive
		if (seat == NUM_SEATS + 1) {
			semWait(rideOver[carID]);

			// now signal to all passengers in car that ride is over.
			seat = 0;
			int i;
			for (i = 0; i < NUM_SEATS; i++) {
				semSignal(riders[i]);
			}

			// signal driver
			semSignal(driverRideOverSem);
			carDriver = 0;
		}
	}

	return 0;

} // end carTask()

// ***********************************************************************
// ***********************************************************************
// visitorTask(int argc, char* argv[])
int visitorTask(int argc, char* argv[]) {


int visitorID = atoi(argv[1]);

	// wait a random number of seconds before arriving (10 second period)
	waitRandSecBetween(1, 10, argv);                          // wait before being outside of park

	// arrive at park
	semWait(parkMutex);                                // protect shared memory access
	changeParkGlobalInt(&myPark.numOutsidePark, 1);  		// enter outside of park
	semSignal(parkMutex);                              // release protect shared memory access

	// wait a few secodns outside the park
	waitRandSecBetween(1, 3, argv);

	// attempt to enter by waiting on inThePark
	semWait(inThePark);                                     // semaphore MAX_IN_PARK wait
	semWait(parkMutex);                                // protect shared memory access
	changeParkGlobalInt(&myPark.numOutsidePark, -1); 		// leave outside of park
	changeParkGlobalInt(&myPark.numInPark, 1);               // enter park

	// get in line for tickets ( and wait 3 sec max)
	changeParkGlobalInt(&myPark.numInTicketLine, 1); 		// enter ticket line
	semSignal(parkMutex);                              // release protect shared memory access
	waitRandSecBetween(1, 3, argv);                           // wait up to 3 seconds

	// attempt to purchase ticket by:
	// first wake up drivers.
	semSignal(driverSleep);
	printf("visitor %d wake up drivers!\n",visitorID);

	// wait for ticket to be ready
	semWait(ticketDataReady);
	printf("visitor %d sees's a ticket ready!\n", visitorID);

	// confirm back to driver that ticket was received
	semSignal(ticketGiven);
	printf("visitor %d got the ticket!\n", visitorID);

	// wait in museum line 3 sec max
	semWait(parkMutex);                		                // protect shared memory access
	changeParkGlobalInt(&myPark.numInTicketLine, -1);		// leave ticket line
	changeParkGlobalInt(&myPark.numInMuseumLine, 1);			// enter museum line
	semSignal(parkMutex);               	               // release protect shared memory access
	waitRandSecBetween(1, 3, argv);                           // wait up to 3 seconds.

	// go into museum 3 sec max
	semWait(museum);                                        // semaphore MAX_IN_MUSEUM wait.
	semWait(parkMutex);                                // protect shared memory access
	changeParkGlobalInt(&myPark.numInMuseumLine, -1);		// leave museum line
	changeParkGlobalInt(&myPark.numInMuseum, 1);             // enter museum
	semSignal(parkMutex);                              // release protect shared memory access
	waitRandSecBetween(1, 3, argv);                           // wait up to 3 seconds

	// get into tour car line
	semWait(parkMutex);                                // protect shared memory access
	changeParkGlobalInt(&myPark.numInMuseum, -1);    		// leave museum
	changeParkGlobalInt(&myPark.numInCarLine, 1);    		// enter car line
	semSignal(parkMutex);                              // release protect shared memory access
	semSignal(museum);                                      // semaphore MAX_IN_MUSEUM post;

	// wait a few seconds in tour car line
	waitRandSecBetween(1, 3, argv);

	// wait for available car and attempt to baord
	semWait(getPassenger);									// wait for car to want passenger
	semWait(seatLock);										// block the seat.

	// create a semaphore unique to each visitor that will wait until the ride is over.
	char visitorName[20];
	sprintf(visitorName, "visitor%d", visitorID);			//guarantee unique semID
	Semaphore * visitorSemID;
	visitorSemID = createSemaphore(visitorName, BINARY, 0);

	// set self semaphore to be passed to car
	nextPassenger = visitorSemID;
	semSignal(seatTaken);									// tell car that seat has been claimed
	semWait(passengerSeated);							// wait for car's ackowledgement of seat
	semSignal(seatLock);									// stop blocking seat.

	// board tour car and release ticket
	semWait(parkMutex);                                // protect shared memory access
	changeParkGlobalInt(&myPark.numInCarLine, -1);   		// leave car line
	changeParkGlobalInt(&myPark.numInCars, 1);               // enter car
	changeParkGlobalInt(&myPark.numTicketsAvailable, 1);		// increment num tickets available
	semSignal(parkMutex);                              // release protect shared memory access
	semSignal(ticket);                                      // release ticket.

	// do tour and wait for ride to be over
	semWait(visitorSemID);

	// delete semaphore that was used to wait for the ride
	deleteSemaphore(&visitorSemID);

	// leave tour car
	semWait(parkMutex);                                // protect shared memory access
	changeParkGlobalInt(&myPark.numInCars, -1);              // leave car

	// enter gift shop line (3 sec max)
	changeParkGlobalInt(&myPark.numInGiftLine, 1);   		// enter gift shop line
	semSignal(parkMutex);                              // release protect shared memory access
	waitRandSecBetween(1, 3, argv);                           // wait up to 3 secs in line

	// go into gift shop (3 sec max)
	semWait(giftShop);                                      // semaphore for gift shop wait
	semWait(parkMutex);                                // protect shared memory access
	changeParkGlobalInt(&myPark.numInGiftLine, -1);  		// leave gift shop line
	changeParkGlobalInt(&myPark.numInGiftShop, 1);   		// enter gift shop
	semSignal(parkMutex);                              // release protect shared memory access

	// leave gift shop
	waitRandSecBetween(1, 3, argv);                           // wait up to 3 seconds in gift shop
	semWait(parkMutex);                                // protect shared memory access
	changeParkGlobalInt(&myPark.numInGiftShop, -1);  		// leave gift shop

	// exit park
	changeParkGlobalInt(&myPark.numInPark, -1);              // leave in park
	changeParkGlobalInt(&myPark.numExitedPark, 1);   		// exit park
	semSignal(giftShop);                                    // semaphore gift shop signal
	semSignal(inThePark);
	semSignal(parkMutex);                              // release protect shared memory access
	return 0;
} // end visitorTask

// ***********************************************************************
// ***********************************************************************
// driverTask
int driverTask(int argc, char* argv[]) {

	int driverID = atoi(argv[1]);
	int semToWaitRide = 0;

	while (TRUE) {
		swapTask();

		//printf("driver %d alive!\n",driverID);
		// wait to be woken up
		semWait(driverSleep);
		//printf("driver %d awake!\n",driverID);
		// lock other drivers from accessing critical code
		semWait(driverLock);
		//printf("driver %d inside!\n",driverID);
		// either sell an available ticket
		if ((myPark.numInTicketLine > 0) && semTryLock(ticket)) {
			printf("driver %d passed ticket check!\n",driverID);
			// set driver function to selling tickets
			semWait(parkMutex);
			myPark.drivers[driverID] = -1;
			semSignal(parkMutex);

			// sell ticket
			semWait(parkMutex);                                // protect shared memory access
			changeParkGlobalInt(&myPark.numTicketsAvailable, -1); // decrement num tickets available
			semSignal(parkMutex);                          // release protect shared memory access

			// tell visitor that ticket has been sold
			semSignal(ticketDataReady);
			printf("driver %d signaled ticketDataReady!\n",driverID);
			// wait for visitor's confirmation
			semWait(ticketGiven);
			printf("driver %d got ticket confirmation!\n",driverID);
			// set driver function to stop selling tickets
			semWait(parkMutex);
			myPark.drivers[driverID] = 0;
			semSignal(parkMutex);

			// release protection of critical driver code
			semSignal(driverLock);
			printf("driver %d finished!\n",driverID);
		}

		// or else fill driver seat in car and wait for rideOver
		else if (semTryLock(carReadyForDriver)) {
			//printf("driver %d passed car!\n",driverID);
			// set global to let car know what driver will sit
			nextCarDriver = driverID;

			// tell car that driver is ready to sit
			semSignal(carDataReady);
			//printf("driver %d told car that driver ready!\n",driverID);

			// wait for car to confirm sriver sitting
			semWait(carGiven);
			printf("driver %d got car confirmation!\n",driverID);

			// set driver function to car driving
			semWait(parkMutex);
			myPark.drivers[driverID] = carForDriver + 1;
			semToWaitRide = carForDriverSem;
			semSignal(parkMutex);

			printf("driver %d info changed!\n",driverID);

			semSignal(driverForCarSet);
			printf("driver %d signaled car for it's change!\n",driverID);
			semWait(driverForCarAck);
			printf("driver %d got car confirmation!\n",driverID);

			// release protection of critical driver code
			semSignal(driverLock);
			printf("driver %d released lock!\n",driverID);

			// wait for rideOver
			semWait(semToWaitRide);
			printf("driver %d ride over!\n",driverID);

		} else {

			// release protection of critical driver code
			semSignal(driverLock);

			// if was awoken but didn't do anything, remember that something needs to be done.
			semSignal(driverSleep);

		}

		// go back to sleep
		semWait(parkMutex);
		myPark.drivers[driverID] = 0;
		semSignal(parkMutex);
	}
	return 0;
} // end driverTask

// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc_test(int argc, char* argv[]) {
	testDeltaClock(argc, argv);
	return 0;
}

// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[]) {
	listDeltaClock();
	return 0;
} // end CL3_dc

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

	if (TRUE) {
		//printf("Inserting Task %d into delta clock with time=%d\n", taskId, eventTime);
	}

	// Create a Clock Event Blocking on this task's semaphore
	if (insertDeltaClock(eventTime, semId) < 0) {
		printf("There was an error trying to insert Semaphore %s with time %d\n", semId->name,
				eventTime);
	}

	// Wait
	semWait(semId);

	clock_gettime(CLOCK_MONOTONIC, &tend);
	double difference = ((double) tend.tv_sec + 1.0e-9 * tend.tv_nsec)
			- ((double) tstart.tv_sec + 1.0e-9 * tstart.tv_nsec);

	printf("Task %d end. total time = %.5f, arg = %d ; diff = %.3f.\n", taskId, difference,
			eventTime, difference - ((double) eventTime / 10));
	free(name);
	return 0;
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


 // ***********************************************************************
 // display all pending events in the delta clock list
 void printDeltaClock(void)
 {
 int i;
 for (i=0; i<numDeltaClock; i++)
 {
 printf("\n%4d%4d  %-20s", i, deltaClock[i].time, deltaClock[i].sem->name);
 }
 return;
 }


 // ***********************************************************************
 // test delta clock
 int P3_tdc(int argc, char* argv[])
 {
 createTask( "DC Test",			// task name
 dcMonitorTask,		// task
 10,					// task priority
 argc,					// task arguments
 argv);

 timeTaskID = createTask( "Time",		// task name
 timeTask,	// task
 10,			// task priority
 argc,			// task arguments
 argv);
 return 0;
 } // end P3_tdc



 // ***********************************************************************
 // monitor the delta clock task
 int dcMonitorTask(int argc, char* argv[])
 {
 int i, flg;
 char buf[32];
 // create some test times for event[0-9]
 int ttime[10] = {
 90, 300, 50, 170, 340, 300, 50, 300, 40, 110	};

 for (i=0; i<10; i++)
 {
 sprintf(buf, "event[%d]", i);
 event[i] = createSemaphore(buf, BINARY, 0);
 insertDeltaClock(ttime[i], event[i]);
 }
 printDeltaClock();

 while (numDeltaClock > 0)
 {
 SEM_WAIT(dcChange)
 flg = 0;
 for (i=0; i<10; i++)
 {
 if (event[i]->state ==1)			{
 printf("\n  event[%d] signaled", i);
 event[i]->state = 0;
 flg = 1;
 }
 }
 if (flg) printDeltaClock();
 }
 printf("\nNo more events in Delta Clock");

 // kill dcMonitorTask
 tcb[timeTaskID].state = S_EXIT;
 return 0;
 } // end dcMonitorTask


 extern Semaphore* tics1sec;

 // ********************************************************************************************
 // display time every tics1sec
 int timeTask(int argc, char* argv[])
 {
 char svtime[64];						// ascii current time
 while (1)
 {
 SEM_WAIT(tics1sec)
 printf("\nTime = %s", myTime(svtime));
 }
 return 0;
 } // end timeTask
 */

