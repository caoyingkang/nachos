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

#include "system.h"
#include "filehdr.h"
#include "utility.h"

static const char *FileTypeName[6] = {"DIR", "EXE", "TXT", "CC", "BIT", "UNK"};

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//  "t" is the type of this file
//----------------------------------------------------------------------
bool
FileHeader::Allocate(BitMap *freeMap, int fileSize, FileType t)
{
    int i, k;

    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    type = t;
    getCurrTime(create_time);
    getCurrTime(visit_time);
    getCurrTime(modify_time);

    int num_indr = divRoundUp(numSectors, SectorSize / sizeof(int));
    ASSERT(num_indr <= NumIndirect);

    if (freeMap->NumClear() < numSectors + num_indr)
	    return FALSE;		// not enough space

    int len = SectorSize / sizeof(int);
    int *sectors = new int[len];
    for (i = 0; i < num_indr; i++) {
	    indirectSectors[i] = freeMap->Find();
        if (i != num_indr - 1)
            for (k = 0; k < len; k++)
                sectors[k] = freeMap->Find();
        else {
            for (k = 0; k < (numSectors - len * (num_indr - 1)); k++)
                sectors[k] = freeMap->Find();
            for (; k < len; k++)
                sectors[k] = -1;
        }
        synchDisk->WriteSector(indirectSectors[i], (char *)sectors);
    }
    for (; i < NumIndirect; i++) {
        indirectSectors[i] = -1;
    }
    delete[] sectors;

    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------
void 
FileHeader::Deallocate(BitMap *freeMap)
{
    int i, k;
    int num_indr = divRoundUp(numSectors, SectorSize / sizeof(int));

    int len = SectorSize / sizeof(int);
    int *sectors = new int[len];
    for (i = 0; i < num_indr; i++) {
        ASSERT(freeMap->Test(indirectSectors[i])); // ought to be marked!
        synchDisk->ReadSector(indirectSectors[i], (char *)sectors);
        for (k = 0; k < len; k++) {
            if (sectors[k] == -1)
                break;
            ASSERT(freeMap->Test(sectors[k])); // ought to be marked!
            freeMap->Clear(sectors[k]);
        }
        freeMap->Clear(indirectSectors[i]);
    }
    delete[] sectors;
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
    synchDisk->ReadSector(sector, (char *)this);
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
    synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//  This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------
int
FileHeader::ByteToSector(int offset)
{
    int len = SectorSize / sizeof(int);
    int *sectors = new int[len];

    int i = offset / (len * SectorSize);
    int k = (offset % (len * SectorSize)) / SectorSize;
    ASSERT(indirectSectors[i] != -1);
    synchDisk->ReadSector(indirectSectors[i], (char *)sectors);
    ASSERT(sectors[k] != -1);

    int ret = sectors[k];
    delete[] sectors;
    return ret;
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
    int i, j, k, bytes;
    char *data = new char[SectorSize];

    int len = SectorSize / sizeof(int);
    int *sectors = new int[len];
    int num_indr = divRoundUp(numSectors, len);

    printf("FileHeader contents: \n\tFile type: %s. File size: %d.\n\tFile blocks: ", 
            FileTypeName[(int)type], numBytes);
    for (i = 0; i < num_indr; i++) {
	    printf("(%d), ", indirectSectors[i]);
        synchDisk->ReadSector(indirectSectors[i], (char *)sectors);
        for (k = 0; k < len; k++) {
            if (sectors[k] == -1)
                break;
    	    printf("%d, ", sectors[k]);         
        }
    }

    printf("\n\tCreated time: %s.\n\tLast visited time: %s.\n\tLast modified time: %s.\n",
            create_time, visit_time, modify_time);

    printf("File contents:\n\t");
    bytes = 0;
    for (i = 0; i < num_indr; i++) {
        synchDisk->ReadSector(indirectSectors[i], (char *)sectors);
        for (k = 0; k < len; k++) {
            if (sectors[k] == -1)
                break;
            synchDisk->ReadSector(sectors[k], data);
            for (j = 0; (j < SectorSize) && (bytes < numBytes); j++, bytes++) {
                if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                    printf("%c", data[j]);
                else
                    printf("\\%x", (unsigned char)data[j]);
            }
        }
    }
    printf("\n"); 
    delete[] sectors;

    delete[] data;
}

//----------------------------------------------------------------------
// GetFileType
// 	Get file type from the file name.
//  If name ends with ".txt", ".cc", the type is recognized.
//  All other cases lead to UNK type.
//----------------------------------------------------------------------
FileType GetFileType(char *name) {
    int len = strlen(name);
    int i;
    for (i = len - 1; i >= 0; i--) {
        if (name[i] == '.')
            break;
    }
    if (i == -1) // no extension
        return UNK;
    if (!strcmp(name + i, ".txt"))
        return TXT;
    if (!strcmp(name + i, ".cc"))
        return CC;
    return UNK;
}
