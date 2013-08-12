// os345mmu.c - LC-3 Memory Management Unit
// **************************************************************************
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
#include "os345lc3.h"

// ***********************************************************************
// mmu variables

// LC-3 memory
unsigned short int memory[LC3_MAX_MEMORY];

// statistics
int memAccess;							// memory accesses
int memHits;							// memory hits
int memPageFaults;					// memory faults
int nextPage;							// swap page size
int pageReads;							// page reads
int pageWrites;						// page writes
int totalFrame;
int nextupt = 0;
int nextrpt = 0;
extern int curTask;
extern TCB tcb[MAX_TASKS];
int getFrame(int);
int getAvailableFrame(int);
int getClockFrame(int);

int getFrame(int notme) {
	int frame;
	frame = getAvailableFrame(notme);
	//printf("getFrame %d",frame);
	if (frame >= 0)
		return frame;

	// run clock
	//printf("\nWe're toast!!!!!!!!!!!!");
	frame = getClockFrame(notme);

	return frame;
}
// **************************************************************************
// **************************************************************************
// LC3 Memory Management Unit
// Virtual Memory Process
// **************************************************************************
//           ___________________________________Frame defined
//          / __________________________________Dirty frame
//         / / _________________________________Referenced frame
//        / / / ________________________________Pinned in memory
//       / / / /     ___________________________
//      / / / /     /                 __________frame # (0-1023) (2^10)
//     / / / /     /                 / _________page defined
//    / / / /     /                 / /       __page # (0-4096) (2^12)
//   / / / /     /                 / /       /
//  / / / /     / 	             / /       /
// F D R P - - f f|f f f f f f f f|S - - - p p p p|p p p p p p p p

unsigned short int *getMemAdr(int va, int rwFlg) {
	unsigned short int pa;
	int rpta, rpte1, rpte2;
	int upta, upte1, upte2;
	int rptFrame, uptFrame;
	memAccess += 2;
	rpta = TASK_RPT+RPTI(va);
	rpte1 = memory[rpta];
	rpte2 = memory[rpta + 1];
	//return &memory[va];

	// turn off virtual addressing for system RAM
	if (va < 0x3000)
		return &memory[va];
	if (DEFINED(rpte1)) {
		// defined
		rptFrame = FRAME(rpte1);
		memHits++;
	} else {
		memPageFaults++;
		// fault
		rptFrame = getFrame(-1);
		//accessPage(nextPage,rptFrame,PAGE_FREE);
		rpte1 = SET_DEFINED(SET_DIRTY(rptFrame));
		if (PAGED(rpte2)) {
			accessPage(SWAPPAGE(rpte2), rptFrame, PAGE_READ);
		} else {
			rpte1 = SET_DIRTY(rpte1);
			rpte2 = 0;
			//for (i=0; i<64; i++) memory[(rptFrame<<6) + i] = 0;
			memset(&memory[(rptFrame << 6)], 0, 128);
		}
	}

	memory[rpta] = rpte1 = SET_REF(SET_PINNED(rpte1));
	memory[rpta + 1] = rpte2;

	upta = (FRAME(rpte1) << 6) + UPTI(va);
	upte1 = memory[upta];
	upte2 = memory[upta + 1];

	if (DEFINED(upte1)) {
		// defined
		uptFrame = FRAME(upte1);
		memHits++;
	} else {
		// fault
		uptFrame = getFrame(rptFrame);
		//accessPage(nextPage,uptFrame,PAGE_FREE);
		upte1 = SET_DEFINED( (uptFrame));
		if (PAGED(upte2)) {
			accessPage(SWAPPAGE(upte2), uptFrame, PAGE_READ);
		} else {
			upte1 = SET_DIRTY(upte1);
			upte2 = 0;
		}
		memPageFaults++;
	}
	if (rwFlg) {
		upte1 = SET_DIRTY(upte1);

	}

	memory[upta] = SET_REF(upte1);
	memory[upta + 1] = upte2;

	return &memory[(FRAME(upte1) << 6) + FRAMEOFFSET(va)];

} // end getMemAdr

// **************************************************************************
// **************************************************************************
// set frames available from sf to ef
//    flg = 0 -> clear all others
//        = 1 -> just add bits
//
void setFrameTableBits(int flg, int sf, int ef) {
	int i, data;
	int adr = LC3_FBT - 1;             // index to frame bit table
	int fmask = 0x0001;              // bit mask

	// 1024 frames in LC-3 memory
	for (i = 0; i < LC3_FRAMES; i++) {
		if (fmask & 0x0001) {
			fmask = 0x8000;
			adr++;
			data = (flg) ? MEMWORD(adr):0;
		}
		else fmask = fmask >> 1;
		// allocate frame if in range
					if ( (i >= sf) && (i < ef)) data = data | fmask;
					MEMWORD(adr) = data;
				}
	return;
} // end setFrameTableBits

// **************************************************************************
// get frame from frame bit table (else return -1)
int getAvailableFrame(int notme) {
	int i, data;
	int adr = LC3_FBT - 1;				// index to frame bit table
	int fmask = 0x0001;					// bit mask

	for (i = 0; i < LC3_FRAMES; i++)		// look thru all frames
			{
		if (fmask & 0x0001) {
			fmask = 0x8000;				// move to next work
			adr++;
			data = MEMWORD(adr);
		}
		else fmask = fmask >> 1;		// next frame
					// deallocate frame and return frame #
					if (data & fmask)
					{	MEMWORD(adr) = data & ~fmask;
						accessPage(nextPage,FRAME(i),PAGE_FREE);
						return i;
					}
				}
	return -1;
} // end getAvailableFrame

// **************************************************************************
// get frame from frame bit table (else return -1)
int getClockFrame(int notme) {
	//printf("\nTry to Swap out a frame by clock algorithm without %d  rpt %x upt %x",notme,nextrpt,nextupt);
	//printf("\nTask rpt %04x",TASK_RPT);
	//printf("\nframe:%d",totalFrame);
	int rp = 0x2400;	//TASK_RPT;
	int location;
	if (nextrpt) {
		rp = nextrpt;

	}
	int count;
	for (count = 0; count < 13; rp = rp + 2) {	//scan root page table

		if (rp >= 0x3000)	//TASK_RPT+64)
				{
			//printf("\n\n a new loop....");
			rp = 0x2400;	//TASK_RPT;
			count++;
		}

		if (MEMWORD(rp)) { // for each entry, scan into user page table.

			memory[rp]=CLEAR_PINNED(memory[rp]);
			int pte1=MEMWORD(rp);

			int pte2=MEMWORD(rp+1);
			//printf("\nRPT entry at %04x = %04x - %04x frame:%d point to:%x",rp,pte1,pte2,FRAME(pte1),0x3000+(40*(FRAME(pte1)-192)));

			int uptLocation= FRAME(pte1)<<6;// starting point of upt
			//printf("\nStarting uptLocation:%x",uptLocation);
			location=uptLocation;
			/*if(rp==nextrpt&&nextupt!=0)
			 {
			 location=nextupt;
			 nextupt=uptLocation;
			 }*/

			int isempty=1;
			for(;location>=uptLocation&&location<uptLocation+0x40;location+=2)
			{			//iterate through user page table

				if(MEMWORD(location))
				{			//data block found
					memory[rp]=SET_PINNED(memory[rp]);
					isempty=0;
					int upt1=MEMWORD(location);
					int upt2=MEMWORD(location+1);

					//	printf("\n -UPT entry at %04x = %04x - %04x frame:%d point to:%x",location,upt1,upt2,FRAME(upt1),0x3000+(40*(FRAME(upt1)-192)));
					//if r=0, swap this frame
					if((!REFERENCED(upt1))&&(FRAME(upt1)!=notme))
					{

						//set up continue point
						nextrpt=rp;
						nextupt=location;
						//free the frame table bit
						//swap it out
						memory[location]= 0;

						//memory[location+1]=PAGED(memory[location+1]);
						if(PAGED(upt2))
						{
							if(DIRTY(upt1))
							{
								accessPage(SWAPPAGE(upt2),FRAME(upt1),PAGE_OLD_WRITE);
							}

						} else
						{
							memory[location + 1] = SET_PAGED(nextPage);
							accessPage(nextPage,FRAME(upt1),PAGE_NEW_WRITE);
						}
						accessPage(nextPage,FRAME(upt1),PAGE_FREE);
						//printf("\nreturn frame:%d entry:%x %x\n\n",FRAME(upt1),memory[location],memory[location+1]);
						return FRAME(upt1);
					}

					//clear bit after
					if(REFERENCED(memory[location]))
					{

						memory[location]=CLEAR_REF(memory[location]);
						//printf("\n Deref block %x",memory[location]);
					}
				}
			}
			// if the upt is empty, no data block entry
			if(!PINNED(memory[rp])&&FRAME(pte1)!=notme)//REFERENCED(pte1)&&FRAME(pte1)!=notme)
			{
				//if(FRAME(pte1)!=notme)
				{

					//set up continue point
					nextrpt=rp+2;
					nextupt=0;
					//free the table bit
					memory[rp]=0;

					if(PAGED(pte2))
					{
						if(DIRTY(pte1))
						{
							accessPage(SWAPPAGE(pte2),FRAME(pte1),PAGE_OLD_WRITE);
						}
					}
					else
					{
						memory[rp+1]=SET_PAGED(nextPage);
						accessPage(nextPage,FRAME(pte1),PAGE_NEW_WRITE);
					}
					accessPage(nextPage,FRAME(pte1),PAGE_FREE);
					//printf("\nreturn frame: %d\n\n",FRAME(pte1));
					return FRAME(pte1);
				}

			}
			/*	if(FRAME(pte1)==notme)
			 printf("%d",notme);if(isempty)
			 {
			 memory[rp]=CLEAR_REF(memory[rp]);
			 //printf("\n Deref upt %x",pte1);

			 }*/
		}
	}
	return -1;
} // end getAvailableFrame

// **************************************************************************
// read/write to swap space
int accessPage(int pnum, int frame, int rwnFlg) {
	static unsigned short int swapMemory[LC3_MAX_SWAP_MEMORY];

	if ((nextPage >= LC3_MAX_PAGE)|| (pnum >= LC3_MAX_PAGE)){
	printf("\nVirtual Memory Space Exceeded!  (%d)", LC3_MAX_PAGE);
	exit(-4);
}
	int i;
	switch (rwnFlg) {
	case PAGE_INIT:                    		// init paging
		nextPage = 0;
		return 0;

	case PAGE_GET_ADR:                    	// return page address
		return (int) (&swapMemory[pnum << 6]);

	case PAGE_NEW_WRITE:                   // new write (Drops thru to write old)
		pnum = nextPage++;

	case PAGE_OLD_WRITE:                   // write
		//printf("\n    (%d) Write frame %d (memory[%04x]) to page %d", p.PID, frame, frame<<6, pnum);
		memcpy(&swapMemory[pnum << 6], &memory[frame << 6], 1 << 7);
		pageWrites++;
		return pnum;

	case PAGE_READ:                    // read
		//printf("\n    (%d) Read page %d into frame %d (memory[%04x])", p.PID, pnum, frame, frame<<6);
		memcpy(&memory[frame << 6], &swapMemory[pnum << 6], 1 << 7);
		pageReads++;
		return pnum;

	case PAGE_FREE:                   // free page
		for (i = frame << 6; i < (frame + 1) << 6; i++) {
			memory[i] = 0;
		}
		break;
	}
	return pnum;
} // end accessPage
