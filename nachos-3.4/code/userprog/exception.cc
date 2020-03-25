// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if (which == SyscallException) {
        if (type == SC_Halt) {
            DEBUG('a', "Shutdown, initiated by user program.\n");
#ifdef USE_TLB
            // printf("Total times address translation takes place: %d\n", 
            //         machine->tlb_lookup_cnt);
            printf("Total times TLB miss happens: %d\n", 
                    machine->tlb_miss_cnt);
#endif // USE_TLB
            interrupt->Halt();
        } // SC_Halt
        else {
            printf("Unimplemented syscall!\n");
            ASSERT(FALSE);
        }

    } // SyscallException
    
    else if (which == PageFaultException) {
#ifdef USE_TLB
        machine->tlb_miss_cnt++;
        int virtAddr = machine->ReadRegister(BadVAddrReg);
        unsigned int vpn = (unsigned) virtAddr / PageSize;
        TranslationEntry *entry = NULL;
        int i = 0;
        for (; i < TLBSize; i++) // search any invalid entry in TLB
            if (!machine->tlb[i].valid) {
                entry = &machine->tlb[i]; // FOUND!
                break;
            }
        if (entry != NULL) { // found an invalid entry

#ifdef TLB_LRU // update tlb_lru
            int j;
            for (j = 0; j < TLBSize; j++) {
                if (machine->tlb_lru[j] == -1) { 
                    machine->tlb_lru[j] = i;
                    break;
                }
            }
            ASSERT(j < TLBSize); // must found one tlb_lru elem
#endif // TLB_LRU 

            // entry->virtualPage = vpn;
            // entry->physicalPage = vpn;
            // entry->valid = true;
            // entry->readOnly = false;
            // entry->use = false;
            // entry->dirty = false;
            memcpy((void *)entry, (void *)&machine->pageTable[vpn], 
                    sizeof(TranslationEntry));
        } else { // replace one TLB entry

#ifdef TLB_FIFO
            entry = &machine->tlb[machine->tlb_next_repl];
            machine->tlb_next_repl = (machine->tlb_next_repl + 1) % TLBSize;
#else // TLB_LRU
            entry = &machine->tlb[machine->tlb_lru[0]];
#endif // TLB_FIFO

            // entry->virtualPage = vpn;
            // entry->physicalPage = vpn;
            // entry->valid = true;
            // entry->readOnly = false;
            // entry->use = false;
            // entry->dirty = false;
            memcpy((void *)&machine->pageTable[entry->virtualPage], 
                    (void *)entry, sizeof(TranslationEntry));
            memcpy((void *)entry, (void *)&machine->pageTable[vpn], 
                    sizeof(TranslationEntry));
        }
#else // Linear Page Table
        ASSERT(FALSE);
#endif // USE_TLB
    } // PageFaultException
}
