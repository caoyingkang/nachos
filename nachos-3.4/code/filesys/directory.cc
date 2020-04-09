// directory.cc 
//	Routines to manage a directory of file names.
//
//	The directory is a table of fixed length entries; each
//	entry represents a single file, and contains the file name,
//	and the location of the file header on disk.  The fixed size
//	of each directory entry means that we have the restriction
//	of a fixed maximum size for file names.
//
//	The constructor initializes an empty directory of a certain size;
//	we use ReadFrom/WriteBack to fetch the contents of the directory
//	from disk, and to write back any modifications back to disk.
//
//	Also, this implementation has the restriction that the size
//	of the directory cannot expand.  In other words, once all the
//	entries in the directory are used, no more files can be created.
//	Fixing this is one of the parts to the assignment.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "filehdr.h"
#include "directory.h"

//----------------------------------------------------------------------
// Directory::Directory
// 	Initialize a directory; initially, the directory is completely
//	empty.  If the disk is being formatted, an empty directory
//	is all we need, but otherwise, we need to call FetchFrom in order
//	to initialize it from disk.
//
//	"size" is the number of entries in the directory
//----------------------------------------------------------------------
Directory::Directory(int size)
{
    // make sure they are of same size
    ASSERT(sizeof(DirectoryEntry) == sizeof(LongFileNameDirEntry));

    table = new DirectoryEntry[size];
    tableSize = size;
    for (int i = 0; i < tableSize; i++)
	    table[i].inUse = FALSE;
}

//----------------------------------------------------------------------
// Directory::~Directory
// 	De-allocate directory data structure.
//----------------------------------------------------------------------
Directory::~Directory()
{ 
    delete [] table;
} 

//----------------------------------------------------------------------
// Directory::FetchFrom
// 	Read the contents of the directory from disk.
//
//	"file" -- file containing the directory contents
//----------------------------------------------------------------------
void
Directory::FetchFrom(OpenFile *file)
{
    (void) file->ReadAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::WriteBack
// 	Write any modifications to the directory back to disk
//
//	"file" -- file to contain the new directory contents
//----------------------------------------------------------------------
void
Directory::WriteBack(OpenFile *file)
{
    (void) file->WriteAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::FindIndex
// 	Look up file name in directory, and return its location in the table of
//	directory entries.  Return -1 if the name isn't in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------
int
Directory::FindIndex(char *name)
{
    // int i, k, offset;
    // LongFileNameDirEntry *entry;
    int len = strlen(name);
    char *str = new char[len + 1];
    for (i = 0; i < tableSize; i++) {
        if (!(table[i].inUse && table[i].normal))
            continue;
        if (len != table[i].nameLen)
            continue;

        // get the file name of this entry
        GetFileName(str, i);
        
        // compare with 'name'
        if (!strcmp(str, name)) {
            delete[] str;
            return i; // find the right entry
        }
    }
    delete[] str;
    return -1; // name not in directory
}

//----------------------------------------------------------------------
// Directory::Find
// 	Look up file name in directory, and return the disk sector number
//	where the file's header is stored. Return -1 if the name isn't 
//	in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------
int
Directory::Find(char *name)
{
    int i = FindIndex(name);
    if (i != -1)
	    return table[i].sector;
    return -1;
}

//----------------------------------------------------------------------
// Directory::Add
// 	Add a file into the directory.  Return TRUE if successful;
//	return FALSE if the file name is already in the directory, or if
//	the directory is completely full, and has no more space for
//	additional file names.
//
//	"name" -- the name of the file being added
//	"newSector" -- the disk sector containing the added file's header
//----------------------------------------------------------------------
bool
Directory::Add(char *name, int newSector)
{
    if (FindIndex(name) != -1)
	    return FALSE;

    int i, k, pre_i, offset;
    LongFileNameDirEntry *entry;

    // calculate how many entries we need
    int len = strlen(name);
    int numEntry = 1;
    if (len > ShortFileNameMaxLen)
        numEntry += divRoundUp(len - ShortFileNameMaxLen, LongFileNameEntLen);

    // check if we have enough unused entries
    for (i = 0, k = 0; i < tableSize; i++)
        if (!table[i].inUse)
            if(++k == numEntry) 
                break;
    if (k < numEntry)
        return FALSE; // no enough space in this directory

    // allocate entries
    for (i = 0, k = 0; i < tableSize; i++)
        if (!table[i].inUse) {
            table[i].inUse = TRUE;
            ++k;
            if (k == 1) { // this entry should be DirectoryEntry
                table[i].normal = TRUE;
                table[i].nameLen = len;
                table[i].sector = newSector;
                strncpy(table[i].name, name, ShortFileNameMaxLen);
                offset = ShortFileNameMaxLen;
            } else { // this entry should be LongFileNameDirEntry
                table[i].normal = FALSE;
                table[pre_i].next = i;
                entry = (LongFileNameDirEntry*)(&table[i]);
                strncpy(entry->name, name + offset, LongFileNameEntLen);
                offset += LongFileNameEntLen;
            }
            pre_i = i;
            if (k == numEntry) { // last entry
                table[i].next = -1;
                break;
            }
	    }
    
    // make sure the file is successfully added
    ASSERT(FindIndex(name) != -1);
    return TRUE;
}

//----------------------------------------------------------------------
// Directory::Remove
// 	Remove a file name from the directory.  Return TRUE if successful;
//	return FALSE if the file isn't in the directory. 
//
//	"name" -- the file name to be removed
//----------------------------------------------------------------------
bool
Directory::Remove(char *name)
{ 
    int i = FindIndex(name);

    if (i == -1)
	    return FALSE; 		// name not in directory

    table[i].inUse = FALSE;
    int k = i;
    while (table[k].next != -1) {
        k = table[k].next;
        table[k].inUse = FALSE;
    }
    return TRUE;	
}

//----------------------------------------------------------------------
// Directory::List
// 	List all the file names in the directory. 
//----------------------------------------------------------------------
void
Directory::List()
{
    for (int i = 0; i < tableSize; i++)
        if (table[i].inUse && table[i].normal) {
            char *name = new char[table[i].nameLen + 1];
            GetFileName(name, i);
	        printf("%s\n", name);
            delete[] name;
        }
}

//----------------------------------------------------------------------
// Directory::Print
// 	List all the file names in the directory, their FileHeader locations,
//	and the contents of each file.  For debugging.
//----------------------------------------------------------------------
void
Directory::Print()
{ 
    FileHeader *hdr = new FileHeader;

    printf("Directory contents:\n");
    for (int i = 0; i < tableSize; i++)
        if (table[i].inUse && table[i].normal) {
            char *name = new char[table[i].nameLen + 1];
            GetFileName(name, i);
            printf("Name: %s, Sector: %d\n", name, table[i].sector);
            hdr->FetchFrom(table[i].sector);
            hdr->Print();
            delete[] name;
        }
    printf("\n");
    delete hdr;
}

//----------------------------------------------------------------------
// Directory::GetFileName
// 	Store the full file name of the index entry in str. 
//  Note: make sure that the entry at 'index' is a normal entry!
//  Note: make sure that str has enough space!
//----------------------------------------------------------------------
void
Directory::GetFileName(char *str, int index) {
    ASSERT(table[index].inUse && table[index].normal);
    
    int k, offset;
    LongFileNameDirEntry *entry;

    strcpy(str, table[index].name);
    offset = ShortFileNameMaxLen;
    k = index;
    while (table[k].next != -1) {
        k = table[k].next;
        entry = (LongFileNameDirEntry*)(&table[k]);
        ASSERT(entry->inUse && !entry->normal);
        strcpy(str + offset, entry->name);
        offset += LongFileNameEntLen;
    }

    ASSERT(strlen(str) == table[index].nameLen);
}