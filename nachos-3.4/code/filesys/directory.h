// directory.h 
//	Data structures to manage a UNIX-like directory of file names.
// 
//      A directory is a table of pairs: <file name, sector #>,
//	giving the name of each file in the directory, and 
//	where to find its file header (the data structure describing
//	where to find the file's data blocks) on disk.
//
//      We assume mutual exclusion is provided by the caller.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.


#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "copyright.h"
#include "openfile.h"

#define ShortFileNameMaxLen 11	// maximum len for short file name
#define LongFileNameEntLen (ShortFileNameMaxLen + 2 * sizeof(int)) 
                            // len each LongFileNameDirEntry can store

// The following class defines a "directory entry", representing a file
// in the directory.  Each entry gives the name of the file, and where
// the file's header is to be found on disk.
//
// Internal data structures kept public so that Directory operations can
// access them directly.

class DirectoryEntry {
  public:
  // members common for DirectoryEntry and LongFileNameDirEntry
    bool normal; // is this a normal directory entry, instead of 
                // a LongFileNameDirEntry? should be true!
    bool inUse;	// is this directory entry in use?
    short next; // next LongFileNameDirEntry, -1 denotes the end.
              // for alignment: use short instead of int.

  // members specific to DirectoryEntry
    int nameLen; // total len of file name
    int sector;	// Location on disk to find the FileHeader for this file
    char name[ShortFileNameMaxLen + 1];	// Text name for file, with +1 for 
					// the trailing '\0'
};

// The following class defines a "long file name directory entry", it has
// the same size as DirectoryEntry, i.e. short file name dir entry. Both
// DirectoryEntry and LongFileNameDirEntry are stored in Directory::table,
// type conversion is required to retrieve the right long file name.
// If the file name is within ShortFileNameMaxLen, then only one DirectoryEntry
// is enough. Otherwise, the leading ShortFileNameMaxLen characters are stored 
// in DirectoryEntry and remaining characters in subsequent 
// LongFileNameDirEntries.

class LongFileNameDirEntry {
  public:
  // members common for DirectoryEntry and LongFileNameDirEntry
    bool normal; // is this a normal directory entry, instead of 
                // a LongFileNameDirEntry? should be false!
    bool inUse;	// is this directory entry in use?
    short next; // next LongFileNameDirEntry, -1 denotes the end
              // for alignment: use short instead of int.
  
  // members specific to LongFileNameDirEntry
    char name[LongFileNameEntLen + 1]; // Text name for file, with +1 for 
					// the trailing '\0'
};


#define NumDirEntries 10
#define DirectoryFileSize (sizeof(DirectoryEntry) * NumDirEntries)

// The following class defines a UNIX-like "directory".  Each entry in
// the directory describes a file, and where to find it on disk.
//
// The directory data structure can be stored in memory, or on disk.
// When it is on disk, it is stored as a regular Nachos file.
//
// The constructor initializes a directory structure in memory; the
// FetchFrom/WriteBack operations shuffle the directory information
// from/to disk. 

class Directory {
  public:
    Directory(int size); 		// Initialize an empty directory
					// with space for "size" files
    ~Directory();			// De-allocate the directory

    void FetchFrom(OpenFile *file);  	// Init directory contents from disk
    void WriteBack(OpenFile *file);	// Write modifications to 
					// directory contents back to disk

    int Find(char *name);		// Find the sector number of the 
					// FileHeader for file: "name"

    bool Add(char *name, int newSector);  // Add a file name into the directory

    bool Remove(char *name);		// Remove a file from the directory

    void List(bool recur = false, char *leading = "");
          // Print the names of all the files in the directory
    void Print();			// Verbose print of the contents
					//  of the directory -- all the file
					//  names and their contents.

    bool isEmpty(); // if the directory contains no files ?

  private:
    int tableSize;			// Number of directory entries
    DirectoryEntry *table; // DirectoryEntries or LongFileNameDirEntries

    int FindIndex(char *name);		// Find the index into the directory 
					//  table corresponding to "name"
    void GetFileName(char *str, int index); // store the full file name of the 
                    // index entry in str. Make sure that str has enough space!
};

#endif // DIRECTORY_H
