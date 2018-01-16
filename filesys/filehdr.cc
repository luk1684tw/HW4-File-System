// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "filehdr.h"
#include "debug.h"
#include "synchdisk.h"
#include "main.h"

//----------------------------------------------------------------------
// MP4 mod tag
// FileHeader::FileHeader
//	There is no need to initialize a fileheader,
//	since all the information should be initialized by Allocate or FetchFrom.
//	The purpose of this function is to keep valgrind happy.
//----------------------------------------------------------------------
FileHeader::FileHeader()
{
	numBytes = -1;
	numSectors = -1;
	numLists = -1;
	memset(dataSectorLists, -1, sizeof(dataSectorLists));
}

//----------------------------------------------------------------------
// MP4 mod tag
// FileHeader::~FileHeader
//	Currently, there is not need to do anything in destructor function.
//	However, if you decide to add some "in-core" data in header
//	Always remember to deallocate their space or you will leak memory
//----------------------------------------------------------------------
FileHeader::~FileHeader()
{
	// nothing to do now
}

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(PersistentBitmap *freeMap, int fileSize)
{ 
	char empty[128] = {0};
	numSectors  = divRoundUp(fileSize, SectorSize);
    numBytes = fileSize;
    numLists = divRoundUp(numSectors, SectorsPerList);

    if (freeMap->NumClear() < numSectors)
		return FALSE;		// not enough space

	int SectorsRead = 0;
	for (int i = 0; i < numLists; i++, SectorsRead += SectorsPerList) {
		dataSectorLists[i] = freeMap->FindAndSet();
		ASSERT(dataSectorLists[i] >= 0);

		int lastSector;
		if (SectorsRead + SectorsPerList > NumSectors)
			lastSector = NumSectors;
		else
			lastSector = SectorsRead + SectorsPerList;
		
		int *buffer = new int[SectorsPerList];
		memset(buffer, 0, sizeof(int)*SectorsPerList);
		for (int j = 0; j < lastSector - SectorsRead; j++) {
			buffer[j] = freeMap->FindAndSet();
			kernel->synchDisk->WriteSector(buffer[j], empty);
			ASSERT(buffer[j] >= 0);
		}
		kernel->synchDisk->WriteSector(dataSectorLists[i], (char*) buffer);
		delete [] buffer;
	}
	return true;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(PersistentBitmap *freeMap)
{
	int SectorsRead = 0;
	for (int i = 0; i < numLists; i++, SectorsRead += SectorsPerList) {
		int lastSector;
		if (SectorsRead + SectorsPerList > numSectors)
			lastSector = numSectors;
		else 
			lastSector = SectorsRead + SectorsPerList;

		int* buffer = new int[SectorsPerList];
		kernel->synchDisk->ReadSector(dataSectorLists[i], (char*) buffer);
		for (int j = 0; j < lastSector; j++) {
			ASSERT(freeMap->Test((int) buffer[j]));
			freeMap->Clear((int)buffer[j]);
		}
		delete [] buffer;
	}
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    kernel->synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    kernel->synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    //return(dataSectors[offset / SectorSize]);
	 int sectorID = offset / SectorSize;
     // calculate where is it stored
     int listID = sectorID / SectorsPerList, idInList = sectorID % SectorsPerList;
     // buf to read in
     int *buffer = new int[SectorsPerList];
     kernel->synchDisk->ReadSector(dataSectorLists[listID], (char *) buffer);
     // get the SectorNum
     int retVal = buffer[idInList];
     delete [] buffer;
     return retVal;
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    /*int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    for (i = 0; i < numSectors; i++)
		printf("%d ", dataSectors[i]);
	
    printf("\nFile contents:\n");
    for (i = k = 0; i < numSectors; i++) {
	kernel->synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n"); 
    }
    delete [] data;*/
}
