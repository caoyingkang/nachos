// filesys.cc 
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk 
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them 
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "disk.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known 
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0
#define DirectorySector 	1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number 
// of files that can be loaded onto the disk.
#define FreeMapFileSize 	(NumSectors / BitsInByte)

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).  
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------
FileSystem::FileSystem(bool format)
{
    ASSERT(sizeof(FileHeader) == SectorSize);

    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        BitMap *freeMap = new BitMap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
        FileHeader *mapHdr = new FileHeader;
        FileHeader *dirHdr = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FreeMapSector);	    
        freeMap->Mark(DirectorySector);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!
        ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize, BIT));
        ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize, DIR));

        // Flush the bitmap and directory FileHeaders back to disk
        // We need to do this before we can "Open" the file, since open
        // reads the file header off of disk (and currently the disk has garbage
        // on it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapHdr->WriteBack(FreeMapSector);    
        dirHdr->WriteBack(DirectorySector);

        // OK to open the bitmap and directory files now
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        rootDirFile = new OpenFile(DirectorySector);
     
        // Once we have the files "open", we can write the initial version
        // of each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);	 // flush changes to disk
        directory->WriteBack(rootDirFile);

        if (DebugIsEnabled('f')) {
            freeMap->Print();
            directory->Print();
        }

        delete freeMap; 
        delete directory; 
        delete mapHdr; 
        delete dirHdr;
    } else {
        // if we are not formatting the disk, just open the files representing
        // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        rootDirFile = new OpenFile(DirectorySector);
    }
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk 
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file 
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//  "type" -- type of file to be created
//  Note: Assume that "name" is in absolute path format, i.e. begin with "/".
//----------------------------------------------------------------------
bool
FileSystem::Create(char *name, FileType type)
{
    Directory *directory;
    BitMap *freeMap;
    FileHeader *hdr;
    OpenFile *openFile;
    int sector;
    bool success;
    bool inRootDir;

    DEBUG('f', "Creating file %s\n", name);

    // find the index of the last '/' character in "name"
    int i, len = strlen(name);
    for (i = len - 1; i >= 0 && name[i] != '/'; i--);
    ASSERT(i >= 0 && name[i] == '/');
    inRootDir = (i == 0);

    // open the directory file in which the new file is to be created
    if (inRootDir) {
        openFile = rootDirFile;
    } else {
        name[i] = '\0';
        openFile = Open(name);
        if (openFile == NULL) // path "name" is not valid
            return FALSE;
        name[i] = '/';
    }
    ASSERT(openFile->getFileType() == DIR);

    // read in directory
    directory = new Directory(NumDirEntries);
    directory->FetchFrom(openFile);

    // strip off path-info in "name"
    name += i + 1;

    if (directory->Find(name) != -1)
        success = FALSE;			// file is already in directory
    else { 
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
        sector = freeMap->Find();	// find a sector to hold the file header
    	if (sector == -1) 		
            success = FALSE;		// no free block for file header 
        else if (!directory->Add(name, sector))
            success = FALSE;	// no space in directory
        else {
            hdr = new FileHeader;
            if (!hdr->Allocate(freeMap, ((type == DIR) ? DirectoryFileSize : 0), type))
                success = FALSE;	// no space on disk for data
            else {	
                success = TRUE;
                // everthing worked, flush all changes back to disk
                hdr->WriteBack(sector); 		
                directory->WriteBack(openFile);
                freeMap->WriteBack(freeMapFile);
            }
            delete hdr;
            if (type == DIR) { // initialize the created directory
                OpenFile *dirFile = new OpenFile(sector);
                Directory *dir = new Directory(NumDirEntries);
                dir->WriteBack(dirFile);
                delete dir;
                delete dirFile;
            }
        }
        delete freeMap;
    }
    delete directory;
    if (openFile != rootDirFile)
        delete openFile;
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.  
//
//	"name" -- the path of the file to be opened.
//  Note: Assume that "name" is in absolute path format: 
//        "/dir1/dir2/.../dirn/xxx". (n>=0)
//  If any one of dir1, dir2, ..., dirn does not exist or is not a 
//  directory, return NULL.
//  If xxx does not exist, return NULL.
//----------------------------------------------------------------------
OpenFile *
FileSystem::Open(char *name)
{
    int i, pre_i, len = strlen(name);
    Directory *directory;
    OpenFile *openFile;
    int sector;
    bool success = TRUE;

    // make sure that "name" is in the right format
    ASSERT(len >= 2 && name[0] == '/' && name[len - 1] != '/');

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(rootDirFile);

    char *dirname = new char[len];
    for (i = 1, pre_i = 0; i < len; i++) {
        if (name[i] != '/')
            continue;

        // get directory name
        strncpy(dirname, name + pre_i + 1, i - pre_i - 1);
        dirname[i - pre_i - 1] = '\0'; // add trailing zero

        // find "dirname" in current dir
        sector = directory->Find(dirname);
        if (sector == -1) { // "dirname" does not exist
            success = FALSE;
            break;
        }

        // open next directory "dirname" and read in its contents
        openFile = new OpenFile(sector);
        if (openFile->getFileType() != DIR) { // "dirname" is not a dir
            success = FALSE;
            delete openFile;
            break;
        }
        delete directory; // delete the previous one
        directory = new Directory(NumDirEntries);
        directory->FetchFrom(openFile);
        delete openFile; // close the file
        
        pre_i = i;
    }
    delete[] dirname;
    
    if (success == FALSE) {
        delete directory;
        return NULL;
    }

    DEBUG('f', "Opening file %s\n", name);

    sector = directory->Find(name + pre_i + 1);
    delete directory;
    if (sector >= 0) 		
	    return new OpenFile(sector); // name was found in directory 
    else
        return NULL;
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system or the path "name" is invalid.
//
//	"name" -- the text name of the file to be removed
//  Note: Assume that "name" is in absolute path format, i.e. begin with "/".
//----------------------------------------------------------------------
bool
FileSystem::Remove(char *name)
{
    Directory *directory;
    OpenFile *openFile;
    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector;
    bool success = TRUE;
    bool inRootDir;

    // find the index of the last '/' character in "name"
    int i, len = strlen(name);
    for (i = len - 1; i >= 0 && name[i] != '/'; i--);
    ASSERT(i >= 0 && name[i] == '/');
    inRootDir = (i == 0);

    // open the directory file containing the to-be-removed file
    if (inRootDir) {
        openFile = rootDirFile;
    } else {
        name[i] = '\0';
        openFile = Open(name);
        if (openFile == NULL) // path "name" is not valid
            return FALSE;
        name[i] = '/';
    }
    ASSERT(openFile->getFileType() == DIR);

    // read in directory
    directory = new Directory(NumDirEntries);
    directory->FetchFrom(openFile);

    // strip off path-info in "name"
    name += i + 1;

    sector = directory->Find(name);
    if (sector == -1) {
        success = FALSE; // file not found 
    } else {
        fileHdr = new FileHeader;
        fileHdr->FetchFrom(sector);

        if (fileHdr->getFileType() == DIR) {
            // check if the directory is empty
            OpenFile *rm_of = new OpenFile(sector);
            Directory *rm_dir = new Directory(NumDirEntries);
            rm_dir->FetchFrom(rm_of);
            if (!rm_dir->isEmpty()) { // non-empty, cannot remove it!
                printf("Unable to remove a non-empty directory!\n");
                success = FALSE;
            }
            delete rm_of;
            delete rm_dir;
        }

        if (success) { // yes, we can remove it.
            freeMap = new BitMap(NumSectors);
            freeMap->FetchFrom(freeMapFile);

            fileHdr->Deallocate(freeMap); // remove data blocks
            freeMap->Clear(sector); // remove header block
            directory->Remove(name);

            freeMap->WriteBack(freeMapFile); // flush to disk
            directory->WriteBack(openFile); // flush to disk
            delete freeMap;
        }
        delete fileHdr;
    }
    delete directory;
    if (openFile != rootDirFile)
        delete openFile;
    return success;
} 

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------
void
FileSystem::List()
{
    printf("--------List all files in Nachos file system--------\n");
    printf("(dir) root\n");
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(rootDirFile);
    directory->List(TRUE, "|-----");
    printf("\n");
    delete directory;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------
void
FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("-------------------Bit map file: -----------------------\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("----------------Root directory file: -------------------\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    printf("-------------------Sectors bitmap: ---------------------\n");
    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    printf("-------------------Root directory: ---------------------\n");
    directory->FetchFrom(rootDirFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
} 
