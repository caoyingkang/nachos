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
//////////////////////////////////////////////
// SyscallException
    if (which == SyscallException) {
        int type = machine->ReadRegister(2);
        switch (type) 
        {
        case SC_Halt:
            DEBUG('a', "Shutdown, initiated by user program.\n");
#ifdef USE_TLB
            // printf("Total times address translation takes place: %d\n", 
            //         currentThread->space->tlb_lookup_cnt);
            printf("Total times TLB miss happens: %d\n", 
                    currentThread->space->tlb_miss_cnt);
#endif // USE_TLB
            interrupt->Halt();
            break; // never reached
        
        case SC_Exit:
            int arg1 = machine->ReadRegister(4);
            printf("User program (tid=%d) exiting with code: %d\n", 
                currentThread->getThreadID(), arg1);
#ifdef INV_PG
            machine->PrintInvPageTable();
#endif // INV_PG
            currentThread->Finish();
            break; // never reached
            
        default:
            printf("Unimplemented syscall!\n");
            ASSERT(FALSE);
            break;
        }
    } // SyscallException

//////////////////////////////////////////////
// PageFaultException
    else if (which == PageFaultException) {
        int virtAddr = machine->ReadRegister(BadVAddrReg);
        unsigned int vpn = (unsigned) virtAddr / PageSize;
        TranslationEntry *tlb_entry;
        int i;
        bool load_pgtable = true;

#ifdef USE_TLB // the fault is invoked by TLB miss
        currentThread->space->tlb_miss_cnt++;
        for (tlb_entry = NULL, i = 0; i < TLBSize; i++) // search any invalid entry in TLB
            if (!machine->tlb[i].valid) {
                tlb_entry = &machine->tlb[i]; // FOUND!
                break;
            }
        if (tlb_entry != NULL) { // found an invalid entry

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

#ifdef INV_PG // use inverted page table.
    // TODO
#else // use normal linear page table.
            // Since VM is not supported, this page must be in memory.
            *tlb_entry = machine->pageTable[vpn];
            load_pgtable = false;
#endif // INV_PG

        } else { // replace one TLB entry

#ifdef TLB_FIFO
            tlb_entry = &machine->tlb[machine->tlb_next_repl];
            machine->tlb_next_repl = (machine->tlb_next_repl + 1) % TLBSize;
#else // TLB_LRU
            tlb_entry = &machine->tlb[machine->tlb_lru[0]];
#endif // TLB_FIFO

#ifdef INV_PG // use inverted page table.
    // TODO
#else // use normal linear page table.
            machine->pageTable[tlb_entry->virtualPage] = *tlb_entry;
            *tlb_entry = machine->pageTable[vpn];
            load_pgtable = false;
#endif // INV_PG

        }
#endif // USE_TLB

        if (load_pgtable) { // this page is not in memory yet, we should load it 
                            // from file, and replace one page if necessary, and 
                            // update TLB if TLB is in use.
            // Note: Only using inverted page table can lead us here.
#ifdef INV_PG
            int tid = currentThread->getThreadID();
            // search any invalid page frame in the resident set
            TranslationEntry *pg_entry;
            int ppn = machine->FindInvalidEntry(tid);
            if (ppn != -1) { // found
                pg_entry = &machine->invPageTable[ppn];
#ifdef PG_LRU // update pg_lru
                int j;
                for (j = 0; j < ResSize; j++) {
                    if (currentThread->space->pg_lru[j] == -1) { 
                        currentThread->space->pg_lru[j] = ppn;
                        break;
                    }
                }
                ASSERT(j < ResSize); // must found one pg_lru elem
#endif // PG_LRU 
            } else { // not found, we should replace one page frame
                ppn = machine->FindReplEntry(tid);
                pg_entry = &machine->invPageTable[ppn];
                // write back if necessary
                if (pg_entry->dirty) {
                    machine->swapFiles[tid]->WriteAt(
                        &machine->mainMemory[ppn * PageSize],
                        PageSize, pg_entry->virtualPage * PageSize);                    
                }
            }
            // load page
            machine->swapFiles[tid]->ReadAt(
                &machine->mainMemory[ppn * PageSize],
                PageSize, vpn * PageSize);
            // reset inverted page table
            pg_entry->virtualPage = vpn;
            pg_entry->valid = true;
            pg_entry->readOnly = machine->ro_bmp[tid]->Test(vpn);
            pg_entry->use = false;
            pg_entry->dirty = false;
            ASSERT(pg_entry->tid == tid);

#else 
            ASSERT(FALSE);
#endif // INV_PG
        }

    } // PageFaultException

    else {
        printf("Unimplemented Exception!\n");
        ASSERT(FALSE);
    }
    
}
