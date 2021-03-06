// filehdr.h 
//	Data structures for managing a disk file header.  
//
//	A file header describes where on disk to find the data in a file,
//	along with other information about the file (for instance, its
//	length, owner, etc.)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef FILEHDR_H
#define FILEHDR_H

#include "disk.h"
#include "bitmap.h"
#include <stdint.h>

#define TimeStrLen 20 // "yyyy-mm-xx hh:mm:ss"
#define NumIndirect ((SectorSize - 2 * sizeof(int) - sizeof(FileType) \
                   - 3 * TimeStrLen) / sizeof(int))
#define MaxFileSize ((NumIndirect * SectorSize / sizeof(int)) * SectorSize)

enum FileType : uint32_t
{
  DIR,    // directory
  EXE,    // executable
  TXT,    // text
  CC,     // cpp source file
  BIT,    // bitmap
  FIFO,   // FIFO pipe file
  SWAP,   // Swap file
  UNK     // unknown type
};

// Get file type from the file name
FileType GetFileType(char *name);

// The following class defines the Nachos "file header" (in UNIX terms,  
// the "i-node"), describing where on disk to find all of the data in the file.
// The file header is organized as a simple table of pointers to
// data blocks. 
//
// The file header data structure can be stored in memory or on disk.
// When it is on disk, it is stored in a single sector -- this means
// that we assume the size of this data structure to be the same
// as one disk sector.  Without indirect addressing, this
// limits the maximum file length to just under 4K bytes.
//
// There is no constructor; rather the file header can be initialized
// by allocating blocks for the file (if it is a new file), or by
// reading it from disk.

class FileHeader {
  public:
    bool Allocate(BitMap *bitMap, int fileSize, FileType t);// Initialize a file header, 
						//  including allocating space 
						//  on disk for the file data
    void Deallocate(BitMap *bitMap);  		// De-allocate this file's 
						//  data blocks

    void FetchFrom(int sectorNumber); 	// Initialize file header from disk
    void WriteBack(int sectorNumber); 	// Write modifications to file header
					//  back to disk

    int ByteToSector(int offset);	// Convert a byte offset into the file
					// to the disk sector containing
					// the byte

    int FileLength();			// Return the length of the file 
					// in bytes

    void Print();			// Print the contents of the file.

    FileType getFileType() {return type;}

    bool IncreaseSize(BitMap *freeMap, int inc); // allocate more disk space
                            // so that file size increases by 'inc'

  private:
    int numBytes;			// Number of bytes in the file
    int numSectors;			// Number of data sectors in the file
    int indirectSectors[NumIndirect];
              // Disk sector numbers for each data block in the file.
              // Each entry is the sector of a direct table.
              // -1 denotes unused entry.
    FileType type; // type of the file
    char create_time[TimeStrLen]; // when was the file created
    char visit_time[TimeStrLen]; // last time the file was visited
    char modify_time[TimeStrLen]; // last time the file was modified

    friend class OpenFile;
};

#endif // FILEHDR_H
