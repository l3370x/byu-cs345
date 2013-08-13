// os345fat.c - file management system
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
//
//		11/19/2011	moved getNextDirEntry to P6
//
// ***********************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include "os345.h"
#include "os345fat.h"

// ***********************************************************************
// ***********************************************************************
//	functions to implement in Project 6
//
int fmsCloseFile(int);
int fmsDefineFile(char*, int);
int fmsDeleteFile(char*);
int fmsOpenFile(char*, int);
int fmsReadFile(int, char*, int);
int fmsSeekFile(int, int);
int fmsWriteFile(int, char*, int);

// ***********************************************************************
// ***********************************************************************
//	Support functions available in os345p6.c
//
extern int fmsGetDirEntry(char* fileName, DirEntry* dirEntry);
extern int fmsGetNextDirEntry(int *dirNum, char* mask, DirEntry* dirEntry, int dir);

extern int fmsMount(char* fileName, void* ramDisk);

extern void setFatEntry(int FATindex, unsigned short FAT12ClusEntryVal, unsigned char* FAT);
extern unsigned short getFatEntry(int FATindex, unsigned char* FATtable);

extern int fmsMask(char* mask, char* name, char* ext);
extern void setDirTimeDate(DirEntry* dir);
extern int isValidFileName(char* fileName);
extern void printDirectoryEntry(DirEntry*);
extern void fmsError(int);

extern int fmsReadSector(void* buffer, int sectorNumber);
extern int fmsWriteSector(void* buffer, int sectorNumber);

// ***********************************************************************
// ***********************************************************************
// fms variables
//
// RAM disk
unsigned char RAMDisk[SECTORS_PER_DISK * BYTES_PER_SECTOR];

// File Allocation Tables (FAT1 & FAT2)
unsigned char FAT1[NUM_FAT_SECTORS * BYTES_PER_SECTOR];
unsigned char FAT2[NUM_FAT_SECTORS * BYTES_PER_SECTOR];

char dirPath[128];							// current directory path
FDEntry OFTable[NFILES];					// open file table

extern bool diskMounted;					// disk has been mounted
extern TCB tcb[];							// task control block
extern int curTask;							// current task #

void setNameExt(DirEntry* dir, char* filename);

int hasEmptyDirEntry(int startSector) {
	char buffer[BYTES_PER_SECTOR];
	int errCode;
	memset(buffer, 0, BYTES_PER_SECTOR);
	errCode = fmsReadSector(&buffer, C_2_S(startSector));
	if (errCode) {
		return errCode;
	}
	int k;
	char remd[1];
	memset(remd, 0xe5, 1);
	bool found = FALSE;
	for (k = 0; k < 16; k++) {
		char checkDir[32];
		char emptyDir[32];
		memset(emptyDir, 0, 32);
		memset(checkDir, 0, 32);
		memcpy(checkDir, &buffer[(32 * k)], 32);
		if (memcmp(checkDir, remd, 1) == 0 || memcmp(checkDir, emptyDir, 32) == 0) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		return 1;
	}
	return 0;
}

void convertToUpperCase(char *sPtr) {
	while (*sPtr != '\0') {
		if (islower(*sPtr))
			*sPtr = toupper(*sPtr);
	}
}

int getFreeCluster() {
	int i;
	for (i = 3; i < CLUSTERS_PER_DISK; i++) {
		if (getFatEntry(i, FAT1) == 0) {
			// clear what it points to
			char buffer[BYTES_PER_SECTOR];
			memset(&buffer, 0, BYTES_PER_SECTOR);
			fmsWriteSector(&buffer, i + 31);
			return i;
		}
	}
	return ERR65;	// File Space Full
}

// Return 0 for success; otherwise, return the error number.
int incFileIndex(FDEntry* fdEntry) {
	unsigned short sector, nextCluster;

	fdEntry->fileIndex = fdEntry->fileIndex + 1;
	if (fdEntry->fileIndex > fdEntry->fileSize) {
		fdEntry->fileSize = fdEntry->fileIndex;
	}

	if (fdEntry->fileIndex % BYTES_PER_SECTOR == 0) {
		// Get next cluster number
		nextCluster = getFatEntry(fdEntry->currentCluster, FAT1);

		if (nextCluster == FAT_EOC) {
			if (fdEntry->mode == OPEN_READ) {
				return ERR66;	// End of file
			} else {
				nextCluster = getFreeCluster();
				setFatEntry(fdEntry->currentCluster, nextCluster, FAT1);
				setFatEntry(fdEntry->currentCluster, nextCluster, FAT2);

				setFatEntry(nextCluster, FAT_EOC, FAT1);
				setFatEntry(nextCluster, FAT_EOC, FAT2);
			}
		}

		// Load new sector into buffer
		int errCode = fmsWriteSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster));
		if (errCode != 0)
			return errCode;
		fdEntry->currentCluster = nextCluster;
		sector = C_2_S(nextCluster);
		updateEntry(fdEntry);
		return fmsReadSector(fdEntry->buffer, sector);
	} else {
		return 0;
	}
}

// ***********************************************************************
// ***********************************************************************
// This function closes the open file specified by fileDescriptor.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
//	Return 0 for success, otherwise, return the error number.
//
int fmsCloseFile(int fileDescriptor) {

	// invalid fileDescriptor
	if (fileDescriptor < 0 || fileDescriptor > NFILES) {
		return ERR52;
	}

	FDEntry* fdEntry = &OFTable[fileDescriptor];

	// file not open
	if (fdEntry->name[0] == 0) {
		return ERR63;	// File Not Open
	}

	// TODO illegal access?

	fmsWriteSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster));

	memset(fdEntry->name, 0, sizeof(uint8) * 8);

	return 0;
} // end fmsCloseFile

void setNameExt(DirEntry* dir, char* filename) {
	char name[8];
	char ext[3];
	memset(name, 0x20, 8);
	memset(ext, 0x20, 3);
	char stop[] = ".";
	int diff = 0;
	int dot = strcspn(filename, stop);
	if (dot != strlen(filename)) {
		diff = strlen(filename) - (dot + 1);
		if (diff > 3) {
			diff = 3;
		}
	}
	if (dot > 8) {
		dot = 8 - diff;
	}
	strncpy(name, filename, dot);
	int i;
	for (i = 0; i < 8; i++) {
		name[i] = toupper((unsigned char) name[i]);
	}
	strncpy(ext, filename + dot + 1, diff);
	for (i = 0; i < 3; i++) {
		ext[i] = toupper((unsigned char) ext[i]);
	}
	memcpy(dir->name, name, 8);
	memcpy(dir->extension, ext, 3);

}

// ***********************************************************************
// ***********************************************************************
// If attribute=DIRECTORY, this function creates a new directory
// file directoryName in the current directory.
// The directory entries "." and ".." are also defined.
// It is an error to try and create a directory that already exists.
//
// else, this function creates a new file fileName in the current directory.
// It is an error to try and create a file that already exists.
// The start cluster field should be initialized to cluster 0.  In FAT-12,
// files of size 0 should point to cluster 0 (otherwise chkdsk should report an error).
// Remember to change the start cluster field from 0 to a free cluster when writing to the
// file.
//
// Return 0 for success, otherwise, return the error number.
//
int fmsDefineFile(char* fileName, int attribute) {
	int i;
	int errCode;
	int sector;
	char remd[1];
	memset(remd, 0xe5, 1);

	if (!diskMounted) {
		return ERR72;
	}

	if (!isValidFileName(fileName)) {
		return ERR50;
	}

	DirEntry dirEntry;

	// check if already exists
	errCode = fmsGetDirEntry(fileName, &dirEntry);

	if (errCode != ERR61) {
		bool skipThis = FALSE;
		if (errCode == 0) {
			return ERR60;
		} else {
			skipThis = TRUE;
		}
		if (!skipThis)
			return errCode;
	}

	int freeCluster = getFreeCluster();
	if (freeCluster < 0) {
		return freeCluster;
	}

	if (attribute == DIRECTORY) {

		// make dir under root dir
		DirEntry * newDir = malloc(32);
		newDir->attributes = DIRECTORY;
		setDirTimeDate(newDir);
		newDir->fileSize = 0;
		setNameExt(newDir, fileName);
		newDir->startCluster = freeCluster;

		// create dot and dotdot dirs at cluster freeCluster
		// create buffer for freeCluster
		char clusterBuffer[BYTES_PER_SECTOR];
		memset(clusterBuffer, 0, BYTES_PER_SECTOR);
		// create DirEntries
		DirEntry * dotDir = malloc(32);
		dotDir->attributes = DIRECTORY;
		dotDir->fileSize = 0;
		dotDir->startCluster = freeCluster;
		setDirTimeDate(dotDir);
		char*theDot = ".";
		setNameExt(dotDir, theDot);
		memcpy(dotDir->name, theDot, 1);

		DirEntry * dotdotDir = malloc(32);
		dotdotDir->attributes = DIRECTORY;
		dotdotDir->fileSize = 0;
		dotdotDir->startCluster = CDIR;
		setDirTimeDate(dotdotDir);
		char*theDotDot = "..";
		setNameExt(dotdotDir, theDot);
		memcpy(dotdotDir->name, theDotDot, 2);

		memcpy(&clusterBuffer, dotDir, 32);
		memcpy(&clusterBuffer[32], dotdotDir, 32);
		fmsWriteSector(&clusterBuffer, C_2_S(freeCluster));
		free(dotDir);
		free(dotdotDir);

		char buffer[BYTES_PER_SECTOR];

		if (CDIR== 0) {

			// writecluster

			// add this newDir to the root directory
			// roor dir starts at sector 19, DirEntries are 32 bytes.
			// root dir has 14 sectors @ 16 entries/sector = 224 entries.
			char emptyDir[32];
			memset(emptyDir,0,32);
			// find last entry.
			int k = 0;
			int j = 0;
			bool done = FALSE;
			for(i = 19; i < 33; i++) {
				memset(buffer,0,BYTES_PER_SECTOR);
				errCode = fmsReadSector(&buffer,i);
				if(errCode) {
					return errCode;
				}
				for ( k = 0; k < 16; k++) {
					char checkDir[32];
					memset(checkDir,0,32);
					for (j = 0; j < 32; j++) {
						checkDir[j] = buffer[j+(32*k)];
					}
					if((memcmp(checkDir,emptyDir,32) == 0) || (memcmp(checkDir,remd,1) == 0)) {
						// found first empty dir spot.
						memcpy(&buffer[(32*k)],newDir,32);
						done = TRUE;
						break;
					}
				}
				if(done) {
					break;
				}
			}
			if(!done) {
				return ERR64; // directory full
			}
			errCode = fmsWriteSector(buffer,i);
			if(errCode) {
				return errCode;
			}
			// write fat entry
			setFatEntry(freeCluster,FAT_EOC,FAT1);
			setFatEntry(freeCluster,FAT_EOC,FAT2);

		} else {

			// writecluster

			// add this newDir to the root directory
			// roor dir starts at sector 19, DirEntries are 32 bytes.
			// root dir has 14 sectors @ 16 entries/sector = 224 entries.
			char emptyDir[32];
			memset(emptyDir,0,32);
			// find last entry.
			int k = 0;
			int j = 0;
			bool done = FALSE;
			// get to correct sector
			int previous = CDIR;
			int startSector = getFatEntry(CDIR,FAT1);
			while (startSector != FAT_EOC || hasEmptyDirEntry(previous)) {
				previous = startSector;
				startSector = getFatEntry(startSector,FAT1);
			}
			//printf("the last Sector is %d",previous);
			startSector = previous;

			// ensure that there is a blank DirEntry in Sector
			memset(buffer,0,BYTES_PER_SECTOR);
			errCode = fmsReadSector(&buffer,C_2_S(startSector));
			if(errCode) {
				return errCode;
			}
			for ( k = 0; k < 16; k++) {
				char checkDir[32];
				memset(checkDir,0,32);
				for (j = 0; j < 32; j++) {
					checkDir[j] = buffer[j+(32*k)];
				}
				if((memcmp(checkDir,emptyDir,32) == 0)|| (memcmp(checkDir,remd,1) == 0)) {
					// found first empty dir spot.
					//printf("empty at sector %d and row %d", startSector,k);
					memcpy(&buffer[(32*k)],newDir,32);
					done = TRUE;
					break;
				}
			}
			if(!done) {
				//printf("didn't find empty spot at sector %d",startSector);
				// TODO create new sector, link with startSector, copy newDir.
				int freeCluster2 = getFreeCluster();
				if (freeCluster2 < 0) {
					return freeCluster2;
				}
				setFatEntry(startSector,freeCluster2,FAT1);
				setFatEntry(startSector,freeCluster2,FAT2);
				memset(buffer,0,BYTES_PER_SECTOR);
				memcpy(&buffer,newDir,32);
				setFatEntry(freeCluster2,FAT_EOC,FAT1);
				setFatEntry(freeCluster2,FAT_EOC,FAT2);

			}

			errCode = fmsWriteSector(buffer, C_2_S(startSector));
			if (errCode) {
				return errCode;
			}
			// write fat entry
			setFatEntry(freeCluster, FAT_EOC, FAT1);
			setFatEntry(freeCluster, FAT_EOC, FAT2);
		}
	} else {
		// is a file

		// make file under root dir
		DirEntry * newDir = malloc(32);
		newDir->attributes = ARCHIVE;
		setDirTimeDate(newDir);
		newDir->fileSize = 0;
		setNameExt(newDir, fileName);
		newDir->startCluster = 0;

		char buffer[BYTES_PER_SECTOR];

		if (CDIR== 0) {

			// writecluster

			// add this newDir to the root directory
			// roor dir starts at sector 19, DirEntries are 32 bytes.
			// root dir has 14 sectors @ 16 entries/sector = 224 entries.
			char emptyDir[32];
			memset(emptyDir,0,32);
			// find last entry.
			int k = 0;
			int j = 0;
			bool done = FALSE;
			for(i = 19; i < 33; i++) {
				memset(buffer,0,BYTES_PER_SECTOR);
				errCode = fmsReadSector(&buffer,i);
				if(errCode) {
					return errCode;
				}
				for ( k = 0; k < 16; k++) {
					char checkDir[32];
					memset(checkDir,0,32);
					for (j = 0; j < 32; j++) {
						checkDir[j] = buffer[j+(32*k)];
					}
					if((memcmp(checkDir,emptyDir,32) == 0)|| (memcmp(checkDir,remd,1) == 0)) {
						// found first empty dir spot.
						memcpy(&buffer[(32*k)],newDir,32);
						done = TRUE;
						break;
					}
				}
				if(done) {
					break;
				}
			}
			if(!done) {
				return ERR64; // directory full
			}
			errCode = fmsWriteSector(buffer,i);
			if(errCode) {
				return errCode;
			}

		} else {

			// writecluster

			// add this newDir to the root directory
			// roor dir starts at sector 19, DirEntries are 32 bytes.
			// root dir has 14 sectors @ 16 entries/sector = 224 entries.
			char emptyDir[32];
			memset(emptyDir,0,32);
			// find last entry.
			int k = 0;
			int j = 0;
//			bool done = FALSE;
//			// get to correct sector
//			int previous = CDIR;
//			int startSector = getFatEntry(CDIR,FAT1);
//			while (startSector != FAT_EOC || hasEmptyDirEntry(previous)) {
//				previous = startSector;
//				startSector = getFatEntry(startSector,FAT1);
//			}
//			//printf("the last Sector is %d",previous);
//			startSector = previous;
			int found = FALSE;
			errCode = hasEmptyDirEntry(CDIR);
			int startSector = CDIR;
			if(errCode<0)
			return errCode;
			if(errCode == 0) {
				while (!hasEmptyDirEntry(startSector)) {
					int previous = startSector;
					startSector = getFatEntry(startSector,FAT1);
					if(startSector == FAT_EOC) {
						// dir cluster full
						int freeCluster2 = getFreeCluster();
						if (freeCluster2 < 0) {
							return freeCluster2;
						}
						setFatEntry(previous,freeCluster2,FAT1);
						setFatEntry(previous,freeCluster2,FAT2);
						//printf("fat %d = %d\n",startSector,freeCluster2);

						setFatEntry(freeCluster2,FAT_EOC,FAT1);
						setFatEntry(freeCluster2,FAT_EOC,FAT2);
						//printf("fat %d = %d\n",freeCluster2,FAT_EOC);

						startSector = freeCluster2;
					}
				}
			}

			// ensure that there is a blank DirEntry in Sector
			memset(buffer,0,BYTES_PER_SECTOR);
			errCode = fmsReadSector(&buffer,C_2_S(startSector));
			if(errCode) {
				return errCode;
			}
			bool done;
			for ( k = 0; k < 16; k++) {
				char checkDir[32];
				memset(checkDir,0,32);
				for (j = 0; j < 32; j++) {
					checkDir[j] = buffer[j+(32*k)];
				}
				if((memcmp(checkDir,emptyDir,32) == 0)|| (memcmp(checkDir,remd,1) == 0)) {
					// found first empty dir spot.
					//printf("empty at sector %d and row %d", startSector,k);
					memcpy(&buffer[(32*k)],newDir,32);
					done = TRUE;
					break;
				}
			}
			if(!done) {
//				//printf("didn't find empty spot at sector %d",startSector);
//				// TODO create new sector, link with startSector, copy newDir.
//				int freeCluster2 = getFreeCluster();
//				if (freeCluster2 < 0) {
//					return freeCluster2;
//				}
//				setFatEntry(startSector,freeCluster2,FAT1);
//				setFatEntry(startSector,freeCluster2,FAT2);
//				memset(buffer,0,BYTES_PER_SECTOR);
//				memcpy(&buffer,newDir,32);
//				setFatEntry(freeCluster2,FAT_EOC,FAT1);
//				setFatEntry(freeCluster2,FAT_EOC,FAT2);

			}

			errCode = fmsWriteSector(buffer, C_2_S(startSector));
			if (errCode) {
				return errCode;
			}
		}
	}

	return 0;

}	// end fmsDefineFile

int recursiveDelete(DirEntry * entry) {
	if (entry->attributes == DIRECTORY) {
		char *dot = ".";
		char *dotdot = "..";
		if (strncmp(entry->name, dot, 1) == 0 || strncmp(entry->name, dotdot, 1) == 0) {
			return 0;
		}
		int index = 0;
		char mask[20];
		DirEntry dirEntry;
		int error = 0;
		strcpy(mask, "*.*");

		while (1) {
			error = fmsGetNextDirEntry(&index, mask, &dirEntry, entry->startCluster);
			if (error) {
				if (error != ERR67)
					return error;
				break;
			}
			//printDirectoryEntry(&dirEntry);
			error = recursiveDelete(&dirEntry);
			if (error != 0)
				return error;
		}

		setFatEntry(dirEntry.startCluster, 0, FAT1);
		setFatEntry(dirEntry.startCluster, 0, FAT2);

		int nextFat = entry->startCluster;
		while (nextFat != FAT_EOC) {
			int zeroMe = nextFat;
			nextFat = getFatEntry(nextFat, FAT1);
			setFatEntry(zeroMe, 0, FAT1);
			setFatEntry(zeroMe, 0, FAT2);
		}

	} else {
		if(entry->startCluster==0)
			return 0;
		int nextFat = entry->startCluster;
		while (nextFat != FAT_EOC) {
			int zeroMe = nextFat;
			nextFat = getFatEntry(nextFat, FAT1);
			setFatEntry(zeroMe, 0, FAT1);
			setFatEntry(zeroMe, 0, FAT2);
		}
	}

	return 0;
}

// ***********************************************************************
// ***********************************************************************
// This function deletes the file fileName from the current director.
// The file name should be marked with an "E5" as the first character and the chained
// clusters in FAT 1 reallocated (cleared to 0).
// Return 0 for success; otherwise, return the error number.
//
int fmsDeleteFile(char* fileName) {
	int errCode;
	if (!diskMounted) {
		return ERR72;
	}
	if (!isValidFileName(fileName)) {
		return ERR50;
	}
	bool isFromRoot = FALSE;

	DirEntry dirEntry;

	// check if doesn't exist
	errCode = fmsGetDirEntry(fileName, &dirEntry);
	if (errCode != 0)
		return errCode;
	bool isDirectory = dirEntry.attributes == DIRECTORY;
	int clusterToRem = dirEntry.startCluster;
	//printf("deleting startCluster = %d", dirEntry.startCluster);
	int currentDirSector;
	char remd[1];
	memset(remd, 0xe5, 1);
	DirEntry * checkThis = malloc(32);
	setNameExt(checkThis, fileName);
	char buffer[BYTES_PER_SECTOR];
	memset(&buffer, 0, BYTES_PER_SECTOR);
	int k;
	if (CDIR== 0) {
		bool found = FALSE;
		currentDirSector = 19;
		while (currentDirSector < 32) {
			memset(&buffer,0,BYTES_PER_SECTOR);
			errCode = fmsReadSector(&buffer, currentDirSector);
			if (errCode != 0) {
				return errCode;
			}
			for (k = 0; k < 16; k++) {
				char checkDir[32];
				char name[8];
				char ext[3];
				memset(ext, 0x20, 3);
				memset(name, 0x20, 8);
				memset(checkDir, 0, 32);
				memcpy(checkDir, &buffer[(32 * k)], 32);
				if (memcmp(checkDir, remd, 1) == 0) {
					continue;
				}
				memcpy(name, &checkDir, 8);
				memcpy(ext, &checkDir[8], 3);
				if (memcmp(name, checkThis->name, 8) == 0 && memcmp(ext, checkThis->extension, 3) == 0) {
					found = TRUE;
					break;
				}
			}
			if(found) {
				break;
			}
			currentDirSector = currentDirSector + 1;
		}
		isFromRoot = TRUE;
	} else {
		bool found = FALSE;
		currentDirSector = CDIR;
		while (currentDirSector != FAT_EOC) {
			memset(&buffer,0,BYTES_PER_SECTOR);
			errCode = fmsReadSector(&buffer, currentDirSector+31);
			if (errCode != 0) {
				return errCode;
			}
			for (k = 0; k < 16; k++) {
				char checkDir[32];
				char name[8];
				char ext[3];
				memset(ext, 0x20, 3);
				memset(name, 0x20, 8);
				memset(checkDir, 0, 32);
				memcpy(checkDir, &buffer[(32 * k)], 32);
				if (memcmp(checkDir, remd, 1) == 0) {
					continue;
				}
				memcpy(name, &checkDir, 8);
				memcpy(ext, &checkDir[8], 3);
				if (memcmp(name, checkThis->name, 8) == 0 && memcmp(ext, checkThis->extension, 3) == 0) {
					found = TRUE;
					break;
				}
			}
			if(found) {
				break;
			}
			currentDirSector = getFatEntry(currentDirSector,FAT1);
		}
		currentDirSector = currentDirSector+31;
	}
	//printf("found match at sector %d and k = %d", currentDirSector, k);
	// set 0xe5 at d,k
	memset(&buffer, 0, BYTES_PER_SECTOR);
	errCode = fmsReadSector(&buffer, currentDirSector);
	if (errCode != 0)
		return errCode;
	memcpy(&buffer[(32 * k)], remd, 1);
	fmsWriteSector(&buffer, currentDirSector);

	// 0 fat until EOC
	recursiveDelete(&dirEntry);


	// if making dir empty and not from root, roll back fat
	// TODo check this?

} // end fmsDeleteFile

// ***********************************************************************
// ***********************************************************************
// This function opens the file fileName for access as specified by rwMode.
// It is an error to try to open a file that does not exist.
// The open mode rwMode is defined as follows:
//    0 - Read access only.
//       The file pointer is initialized to the beginning of the file.
//       Writing to this file is not allowed.
//    1 - Write access only.
//       The file pointer is initialized to the beginning of the file.
//       Reading from this file is not allowed.
//    2 - Append access.
//       The file pointer is moved to the end of the file.
//       Reading from this file is not allowed.
//    3 - Read/Write access.
//       The file pointer is initialized to the beginning of the file.
//       Both read and writing to the file is allowed.
// A maximum of 32 files may be open at any one time.
// If successful, return a file descriptor that is used in calling subsequent file
// handling functions; otherwise, return the error number.
//
int fmsOpenFile(char* fileName, int rwMode) {
	int i;
	int errCode;
	int sector;
	DirEntry dirEntry;

	if (!diskMounted) {
		return ERR72;
	}

	// Get directory entry for fileName
	errCode = fmsGetDirEntry(fileName, &dirEntry);

	if (errCode != 0) {
		return errCode;
	}

	// check if already open
	for (i = 0; i < NFILES; i++) {
		//printf("of = %s, dir = %s",OFTable[i].name,dirEntry.name);
		char * of = OFTable[i].name;
		char * dir = dirEntry.name;
		if (strncmp(OFTable[i].name, dirEntry.name, 8) == 0) {
			return ERR62;
		}
	}

	// Store in Open File Table
	for (i = 0; i < NFILES; i++) {
		if (!OFTable[i].name[0]) {
			// Found open slot in Open File Table
			memcpy(OFTable[i].name, dirEntry.name, 8 * sizeof(uint8));
			memcpy(OFTable[i].extension, dirEntry.extension, 3 * sizeof(uint8));
			OFTable[i].attributes = dirEntry.attributes;
			OFTable[i].directoryCluster = CDIR;
			OFTable[i].startCluster = dirEntry.startCluster;
			OFTable[i].currentCluster = dirEntry.startCluster;
			OFTable[i].fileSize = (rwMode == 1) ? 0 : dirEntry.fileSize;
			OFTable[i].pid = curTask;
			OFTable[i].mode = rwMode;
			OFTable[i].flags = 0;
			OFTable[i].fileIndex = (rwMode != 2) ? 0 : dirEntry.fileSize;

			sector = C_2_S(OFTable[i].startCluster);
			errCode = fmsReadSector(OFTable[i].buffer, sector);
			if (errCode != 0) {
				return errCode;
			}

			return i;
		}
	}

	// Too many files open
	return ERR70;
} // end fmsOpenFile

int fmsReadFile(int fileDescriptor, char* buffer, int nBytes) {
	if (!diskMounted) {
		return ERR72;
	}

	// invalid fileDescriptor
	if (fileDescriptor < 0 || fileDescriptor > NFILES) {
		return ERR52;
	}

	FDEntry* fdEntry = &OFTable[fileDescriptor];

	// file not open
	if (fdEntry->name[0] == 0) {
		return ERR63;	// File Not Open
	}

	// illegal access
	if (fdEntry->mode == 1 || fdEntry->mode == 2) {
		return ERR85;
	}

	// end of file
	if (fdEntry->fileIndex > fdEntry->fileSize) {
		return ERR66;	// End of file
	}
	char myBuffer[4];
	int i = 0;
	int toRead = nBytes;
	if (toRead == 0)
		return i;
	while ((nBytes = badReadFile(fileDescriptor, myBuffer, 1)) == 1) {
		buffer[i] = myBuffer[0];
		i = i + 1;
		if (i >= toRead)
			break;
	}
	return i;
}

// ***********************************************************************
// ***********************************************************************
// This function reads nBytes bytes from the open file specified by fileDescriptor into
// memory pointed to by buffer.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// After each read, the file pointer is advanced.
// Return the number of bytes successfully read (if > 0) or return an error number.
// (If you are already at the end of the file, return EOF error.  ie. you should never
// return a 0.)
//
int badReadFile(int fileDescriptor, char* buffer, int nBytes) {
	int bytesRead, bufferOffset = 0;
	int errorCode;

	if (!diskMounted) {
		return ERR72;
	}

	// invalid fileDescriptor
	if (fileDescriptor < 0 || fileDescriptor > NFILES) {
		return ERR52;
	}

	FDEntry* fdEntry = &OFTable[fileDescriptor];

	// file not open
	if (fdEntry->name[0] == 0) {
		return ERR63;	// File Not Open
	}

	// illegal access
	if (fdEntry->mode == 1 || fdEntry->mode == 2) {
		return ERR85;
	}

	// end of file
	if (fdEntry->fileIndex >= fdEntry->fileSize) {
		return ERR66;	// End of file
	}

	// read
	for (bytesRead = 0; bytesRead < nBytes;) {
		buffer[bufferOffset] = fdEntry->buffer[fdEntry->fileIndex % BYTES_PER_SECTOR];

		// Check for EOF
		if (buffer[bufferOffset] == EOF) {
			return (bytesRead > 0) ? bytesRead : ERR66;		// End of file
		}

		bytesRead++;
		bufferOffset++;

		// Increment fileIndex
		errorCode = incFileIndex(fdEntry);

		if (errorCode == ERR66 && bytesRead > 0) {
			return bytesRead;
		} else if (errorCode) {
			return errorCode;
		}
	}

	return bytesRead;
} // end fmsReadFile

// ***********************************************************************
// ***********************************************************************
// This function changes the current file pointer of the open file specified by
// fileDescriptor to the new file position specified by index.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// The file position may not be positioned beyond the end of the file.
// Return the new position in the file if successful; otherwise, return the error number.
//
int fmsSeekFile(int fileDescriptor, int index) {

	// invalid fileDescriptor
	if (fileDescriptor < 0 || fileDescriptor > NFILES) {
		return ERR52;
	}

	FDEntry* fdEntry = &OFTable[fileDescriptor];

	// file not open
	if (fdEntry->name[0] == 0) {
		return ERR63;	// File Not Open
	}

	// set fdEntry to start
	fdEntry->currentCluster = fdEntry->startCluster;
	fdEntry->fileIndex = 0;

	// load first cluster into buffer.
	char seekedBuffer[BYTES_PER_SECTOR];
	int errCode = fmsReadSector(seekedBuffer, C_2_S(fdEntry->currentCluster));
	if (errCode != 0)
		return errCode;
	memcpy(fdEntry->buffer, seekedBuffer, BYTES_PER_SECTOR);

	// inc index times;
	int i = 0;
	while (i < index) {
		int error = incFileIndex(fdEntry);
		if (error != 0) {
			if (error == ERR66) {
				return i;
			}
			return error;
		}
		i = i + 1;
	}

	return fdEntry->fileIndex;
} // end fmsSeekFile

int updateEntry(FDEntry * fdEntry) {
	DirEntry toFind;
	int errCode;
	bool isFromRoot = FALSE;

	DirEntry dirEntry;

	bool isDirectory = dirEntry.attributes == DIRECTORY;
	int clusterToRem = dirEntry.startCluster;
	//printf("deleting startCluster = %d", dirEntry.startCluster);
	int currentDirSector;
	char remd[1];
	memset(remd, 0xe5, 1);
	char buffer[BYTES_PER_SECTOR];
	memset(&buffer, 0, BYTES_PER_SECTOR);
	int k;
	if (CDIR== 0) {
		bool found = FALSE;
		currentDirSector = 19;
		while (currentDirSector < 32) {
			memset(&buffer,0,BYTES_PER_SECTOR);
			errCode = fmsReadSector(&buffer, currentDirSector);
			if (errCode != 0) {
				return errCode;
			}
			for (k = 0; k < 16; k++) {
				char checkDir[32];
				char name[8];
				char ext[3];
				memset(ext, 0x20, 3);
				memset(name, 0x20, 8);
				memset(checkDir, 0, 32);
				memcpy(checkDir, &buffer[(32 * k)], 32);
				if (memcmp(checkDir, remd, 1) == 0) {
					continue;
				}
				memcpy(name, &checkDir, 8);
				memcpy(ext, &checkDir[8], 3);
				if (memcmp(name, fdEntry->name, 8) == 0 && memcmp(ext, fdEntry->extension, 3) == 0) {
					found = TRUE;
					break;
				}
			}
			if(found) {
				break;
			}
			currentDirSector = currentDirSector + 1;
		}
		isFromRoot = TRUE;
	} else {
		bool found = FALSE;
		currentDirSector = CDIR;
		while (currentDirSector != FAT_EOC) {
			memset(&buffer,0,BYTES_PER_SECTOR);
			errCode = fmsReadSector(&buffer, currentDirSector+31);
			if (errCode != 0) {
				return errCode;
			}
			for (k = 0; k < 16; k++) {
				char checkDir[32];
				char name[8];
				char ext[3];
				memset(ext, 0x20, 3);
				memset(name, 0x20, 8);
				memset(checkDir, 0, 32);
				memcpy(checkDir, &buffer[(32 * k)], 32);
				if (memcmp(checkDir, remd, 1) == 0) {
					continue;
				}
				memcpy(name, &checkDir, 8);
				memcpy(ext, &checkDir[8], 3);
				if (memcmp(name, fdEntry->name, 8) == 0 && memcmp(ext, fdEntry->extension, 3) == 0) {
					found = TRUE;
					break;
				}
			}
			if(found) {
				break;
			}
			currentDirSector = getFatEntry(currentDirSector,FAT1);
		}
		currentDirSector = currentDirSector+31;
	}
	//printf("found match at sector %d and k = %d", currentDirSector, k);
	memset(&buffer, 0, BYTES_PER_SECTOR);
	errCode = fmsReadSector(&buffer, currentDirSector);
	if (errCode != 0)
		return errCode;

	// make file under root dir
	DirEntry * newDir = malloc(32);
	newDir->attributes = fdEntry->attributes;
	setDirTimeDate(newDir);
	memset(newDir->extension, 0x20, 3);
	memset(newDir->name, 0x20, 8);
	newDir->fileSize = fdEntry->fileSize;
	memcpy(newDir->extension, fdEntry->extension, 3);
	memcpy(newDir->name, fdEntry->name, 8);
//	memcpy(OFTable[i].name, dirEntry.name, 8 * sizeof(uint8));
//	memcpy(OFTable[i].extension, dirEntry.extension, 3 * sizeof(uint8));
	newDir->startCluster = fdEntry->startCluster;

	memcpy(&buffer[(32 * k)], newDir, 32);
	fmsWriteSector(&buffer, currentDirSector);
	free(newDir);
	return 0;
}

// ***********************************************************************
// ***********************************************************************
// This function writes nBytes bytes to the open file specified by fileDescriptor from
// memory pointed to by buffer.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// Writing is always "overwriting" not "inserting" in the file and always writes forward
// from the current file pointer position.
// Return the number of bytes successfully written; otherwise, return the error number.
//
int fmsWriteFile(int fileDescriptor, char* buffer, int nBytes) {
	int bytesWritten;
	int errorCode;

	if (!diskMounted) {
		return ERR72;
	}

	// invalid fileDescriptor
	if (fileDescriptor < 0 || fileDescriptor > NFILES) {
		return ERR52;
	}

	FDEntry* fdEntry = &OFTable[fileDescriptor];

	// file not open
	if (fdEntry->name[0] == 0) {
		return ERR63;	// File Not Open
	}

	// illegal access
	if (fdEntry->mode == OPEN_READ) {
		return ERR85;
	}

	// file empty, so get free cluster and assign to fat
	if (fdEntry->startCluster == 0) {
		int freeCluster = getFreeCluster();
		// assign in fat
		setFatEntry(freeCluster, FAT_EOC, FAT1);
		setFatEntry(freeCluster, FAT_EOC, FAT2);
		fdEntry->startCluster = freeCluster;
		fdEntry->currentCluster = freeCluster;
		fdEntry->fileIndex = 0;

		// update OFTable
		int i;
		for (i = 0; i < NFILES; i++) {
			if (strcmp(OFTable[i].name, fdEntry->name) == 0) {
				OFTable[i].currentCluster = freeCluster;
				OFTable[i].startCluster = freeCluster;
			}
		}

		// update directory
		errorCode = updateEntry(fdEntry);
		if (errorCode != 0) {
			return errorCode;
		}

	}

	for (bytesWritten = 0; bytesWritten < nBytes;) {
		fdEntry->buffer[fdEntry->fileIndex % BYTES_PER_SECTOR] = *buffer;
		buffer++;
		bytesWritten++;

		// Increment fileIndex
		errorCode = incFileIndex(fdEntry);

		if (errorCode) {
			return errorCode;
		}
	}

	// write to file
	errorCode = fmsWriteSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster));
	if (errorCode != 0) {
		return errorCode;
	}

	updateEntry(fdEntry);

	return bytesWritten;
} // end fmsWriteFile
