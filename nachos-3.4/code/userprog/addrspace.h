// addrspace.h 
//	Data structures to keep track of executing user programs 
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"

#define UserStackSize		1024 	// increase this as necessary!

class AddrSpace {
  public:
    AddrSpace(OpenFile *executable, int _tid); // Create an address space,
					              // initializing it with the program
					              // stored in the file "executable"
    ~AddrSpace();			// De-allocate an address space

    void InitRegisters();		// Initialize user-level CPU registers,
					// before jumping to user code

    void SaveState();			// Save/restore address space-specific
    void RestoreState();		// info on a context switch

#ifdef USE_TLB
    int tlb_lookup_cnt; // total times TLB is looked up (i.e. the total times 
					// address translation takes place) in this user program
	  int tlb_miss_cnt; // total times TLB misses in this user program
#endif // USE_TLB

#ifdef INV_PG
// page replacement algorithm dependent variables
#ifdef PG_FIFO
	int pg_next_repl; // TODO
#else // PG_LRU
	int pg_lru[ResSize]; // pg_lru[0] is the least recently used (i.e. the one 
			// to be replaced), while pg_lru[ResSize-1] is the most recently used.
			// Note: initialized to all -1, representing invaid entry.
#endif // PG_FIFO
#endif // INV_PG

    unsigned int numPages; // Number of pages in the virtual 
					                // address space

  private:
#ifndef INV_PG // use normal page table, one per user prog. do not support VM.
    TranslationEntry *pageTable;
#endif
};

#endif // ADDRSPACE_H
