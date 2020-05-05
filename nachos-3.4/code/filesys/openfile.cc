// openfile.cc 
//	Routines to manage an open Nachos file.  As in UNIX, a
//	file must be open before we can read or write to it.
//	Once we're all done, we can close it (in Nachos, by deleting
//	the OpenFile data structure).
//
//	Also as in UNIX, for convenience, we keep the file header in
//	memory while the file is open.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "filehdr.h"
#include "openfile.h"
#include "system.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif

extern int of_cnt[];
extern FileHeader *hdrs[];
extern Lock hdrs_lock;
extern Semaphore *rw_sem[];
extern int fread_cnt[];
extern Lock *fread_lock[];

//----------------------------------------------------------------------
// OpenFile::OpenFile
// 	Open a Nachos file for reading and writing.  Bring the file header
//	into memory while the file is open.
//
//	"sector" -- the location on disk of the file header for this file
//----------------------------------------------------------------------
OpenFile::OpenFile(int sector)
{
    seekPosition = 0;
    hdrSector = sector;

    hdrs_lock.Acquire();
    of_cnt[hdrSector]++;
    if (of_cnt[hdrSector] == 1) {
        ASSERT(hdrs[hdrSector] == NULL);
        hdrs[hdrSector] = new FileHeader;
        hdrs[hdrSector]->FetchFrom(hdrSector);

        ASSERT(rw_sem[hdrSector] == NULL);
        rw_sem[hdrSector] = new Semaphore("rw_sem", 1);

        ASSERT(fread_cnt[hdrSector] == 0);
        ASSERT(fread_lock[hdrSector] == NULL);
        fread_lock[hdrSector] = new Lock("fread_lock");
    }
    hdrs_lock.Release();
}

//----------------------------------------------------------------------
// OpenFile::~OpenFile
// 	Close a Nachos file, de-allocating any in-memory data structures.
//----------------------------------------------------------------------
OpenFile::~OpenFile()
{
    hdrs_lock.Acquire();
    of_cnt[hdrSector]--;
    if (of_cnt[hdrSector] == 0) {
        ASSERT(hdrs[hdrSector] != NULL);
        delete hdrs[hdrSector];
        hdrs[hdrSector] = NULL;

        ASSERT(rw_sem[hdrSector] != NULL);
        delete rw_sem[hdrSector];
        rw_sem[hdrSector] = NULL;

        ASSERT(fread_cnt[hdrSector] == 0);
        ASSERT(fread_lock[hdrSector] != NULL);
        delete fread_lock[hdrSector];
        fread_lock[hdrSector] = NULL;
    }
    hdrs_lock.Release();
}

//----------------------------------------------------------------------
// OpenFile::Seek
// 	Change the current location within the open file -- the point at
//	which the next Read or Write will start from.
//
//	"position" -- the location within the file for the next Read/Write
//----------------------------------------------------------------------
void
OpenFile::Seek(int position)
{
    seekPosition = position;
}	

//----------------------------------------------------------------------
// OpenFile::Read/Write
// 	Read/write a portion of a file, starting from seekPosition.
//	Return the number of bytes actually written or read, and as a
//	side effect, increment the current position within the file.
//
//	Implemented using the more primitive ReadAt/WriteAt.
//
//	"into" -- the buffer to contain the data to be read from disk 
//	"from" -- the buffer containing the data to be written to disk 
//	"numBytes" -- the number of bytes to transfer
//----------------------------------------------------------------------
int
OpenFile::Read(char *into, int numBytes)
{
   int result = ReadAt(into, numBytes, seekPosition);
   seekPosition += result;
   return result;
}

int
OpenFile::Write(char *into, int numBytes)
{
   int result = WriteAt(into, numBytes, seekPosition);
   seekPosition += result;
   return result;
}

//----------------------------------------------------------------------
// OpenFile::ReadAt/WriteAt
// 	Read/write a portion of a file, starting at "position".
//	Return the number of bytes actually written or read, but has
//	no side effects (except that Write modifies the file, of course).
//
//	There is no guarantee the request starts or ends on an even disk sector
//	boundary; however the disk only knows how to read/write a whole disk
//	sector at a time.  Thus:
//
//	For ReadAt:
//	   We read in all of the full or partial sectors that are part of the
//	   request, but we only copy the part we are interested in.
//	For WriteAt:
//	   We must first read in any sectors that will be partially written,
//	   so that we don't overwrite the unmodified portion.  We then copy
//	   in the data that will be modified, and write back all the full
//	   or partial sectors that are part of the request.
//
//	"into" -- the buffer to contain the data to be read from disk 
//	"from" -- the buffer containing the data to be written to disk 
//	"numBytes" -- the number of bytes to transfer
//	"position" -- the offset within the file of the first byte to be
//			read/written
//  "calledInWriteAt" -- if this func is called by OpenFile::WriteAt, then 
//     we cannot do P,V operation since otherwise we may get a deadlock.
//----------------------------------------------------------------------
int
OpenFile::ReadAt(char *into, int numBytes, int position, bool calledInWriteAt)
{
    if (!calledInWriteAt) {
        ASSERT(fread_lock[hdrSector] != NULL);

        fread_lock[hdrSector]->Acquire();
        fread_cnt[hdrSector]++;
        if (fread_cnt[hdrSector] == 1)
            rw_sem[hdrSector]->P();
        fread_lock[hdrSector]->Release();
    }

    int fileLength = hdrs[hdrSector]->FileLength();
    int i, firstSector, lastSector, numSectors;
    char *buf;

    if ((numBytes <= 0) || (position >= fileLength)) {
        if (!calledInWriteAt) {
            fread_lock[hdrSector]->Acquire();
            fread_cnt[hdrSector]--;
            if (fread_cnt[hdrSector] == 0)
                rw_sem[hdrSector]->V();
            fread_lock[hdrSector]->Release();
        }
    	return 0; 				// check request
    }
    if ((position + numBytes) > fileLength)
	    numBytes = fileLength - position;
    DEBUG('f', "Reading %d bytes at %d, from file of length %d.\n", 	
			numBytes, position, fileLength);

    firstSector = divRoundDown(position, SectorSize);
    lastSector = divRoundDown(position + numBytes - 1, SectorSize);
    numSectors = 1 + lastSector - firstSector;

    // read in all the full and partial sectors that we need
    buf = new char[numSectors * SectorSize];
    for (i = firstSector; i <= lastSector; i++)	{
        synchDisk->ReadSector(hdrs[hdrSector]->ByteToSector(i * SectorSize), 
					&buf[(i - firstSector) * SectorSize]);
    }
    // copy the part we want
    bcopy(&buf[position - (firstSector * SectorSize)], into, numBytes);
    delete [] buf;

    // update last visited time
    hdrs_lock.Acquire();
    getCurrTime(hdrs[hdrSector]->visit_time);
    hdrs[hdrSector]->WriteBack(hdrSector);
    hdrs_lock.Release();

    if (!calledInWriteAt) {
        fread_lock[hdrSector]->Acquire();
        fread_cnt[hdrSector]--;
        if (fread_cnt[hdrSector] == 0)
            rw_sem[hdrSector]->V();
        fread_lock[hdrSector]->Release();
    }
    return numBytes;
}

int
OpenFile::WriteAt(char *from, int numBytes, int position)
{
    rw_sem[hdrSector]->P();

    int fileLength = hdrs[hdrSector]->FileLength();
    int i, firstSector, lastSector, numSectors;
    bool firstAligned, lastAligned;
    char *buf;

    // extend the file size if necessary
    if (position + numBytes > fileLength) {
        BitMap *freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(fileSystem->freeMapFile);
        if (!hdrs[hdrSector]->IncreaseSize(freeMap, position + numBytes - fileLength)) {
            printf("Unable to extend the size of the file.\n");
            delete freeMap;

            rw_sem[hdrSector]->V();
            return 0;
        }
        freeMap->WriteBack(fileSystem->freeMapFile); // flush changes to disk
        delete freeMap;
    }

    firstSector = divRoundDown(position, SectorSize);
    lastSector = divRoundDown(position + numBytes - 1, SectorSize);
    numSectors = 1 + lastSector - firstSector;

    buf = new char[numSectors * SectorSize];

    firstAligned = (position == (firstSector * SectorSize));
    lastAligned = ((position + numBytes) == ((lastSector + 1) * SectorSize));

// read in first and last sector, if they are to be partially modified
    if (!firstAligned)
        ReadAt(buf, SectorSize, firstSector * SectorSize, TRUE);	
    if (!lastAligned && ((firstSector != lastSector) || firstAligned))
        ReadAt(&buf[(lastSector - firstSector) * SectorSize], 
				SectorSize, lastSector * SectorSize, TRUE);	

// copy in the bytes we want to change 
    bcopy(from, &buf[position - (firstSector * SectorSize)], numBytes);

// write modified sectors back
    for (i = firstSector; i <= lastSector; i++)	{
        synchDisk->WriteSector(hdrs[hdrSector]->ByteToSector(i * SectorSize), 
					&buf[(i - firstSector) * SectorSize]);
    }
    delete [] buf;

    // update last visited time and modified time
    hdrs_lock.Acquire();
    getCurrTime(hdrs[hdrSector]->visit_time);
    getCurrTime(hdrs[hdrSector]->modify_time);
    hdrs[hdrSector]->WriteBack(hdrSector);
    hdrs_lock.Release();

    rw_sem[hdrSector]->V();
    return numBytes;
}

//----------------------------------------------------------------------
// OpenFile::Length
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------
int
OpenFile::Length() 
{ 
    return hdrs[hdrSector]->FileLength(); 
}

//----------------------------------------------------------------------
// OpenFile::getFileType
// 	Return the type of this file
//----------------------------------------------------------------------
FileType
OpenFile::getFileType()
{
    return hdrs[hdrSector]->type;
}