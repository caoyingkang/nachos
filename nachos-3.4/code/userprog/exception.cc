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

// Get string starting at virtual address "addr".
// Don't forget to delete the returned string outside this function!
static char *
getStrArg(int addr)
{
    int data;
    int len = 0;
    // get str length
    while (TRUE) {
        while (TRUE) // allow for possible pagefault exceptions
            if (machine->ReadMem(addr + len, 1, &data))
                break;
        if (((char)data) == '\0')
            break;
        len++;
    }
    // get str
    char *str = new char[len + 1];
    for (int i = 0; i < len; i++) {
        while (TRUE) // allow for possible pagefault exceptions
            if (machine->ReadMem(addr + i, 1, &data))
                break;
        str[i] = (char)data;
    }
    str[len] = '\0';
    return str;
}

// Read "size" bytes into "buff" from simulated memory,
// starting from virtual address "addr".
// Note: make sure that buff is big enough.
static void
ReadMemManyBytes(int addr, int size, char *buff)
{
    int data;
    for (int i = 0; i < size; i++) {
        while (TRUE) // allow for possible pagefault exceptions
            if (machine->ReadMem(addr + i, 1, &data))
                break;
        buff[i] = (char)data;
    }
}

// Write "size" bytes from "buff" to simulated memory,
// starting from virtual address "addr".
static void
WriteMemManyBytes(int addr, int size, char *buff)
{
    for (int i = 0; i < size; i++) {
        while (TRUE) // allow for possible pagefault exceptions
            if (machine->WriteMem(addr + i, 1, (int)(buff[i])))
                break;
    }
}

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
        int arg1, arg2, arg3;
        int len;
        char *str, *filename;
        char *buff;
        OpenFile *openFile;
        switch (type) {

          case SC_Halt:
            DEBUG('a', "In Syscall Halt.\n");
#ifdef USE_TLB
            printf("Total times TLB miss happens: %d\n", 
                    currentThread->space->tlb_miss_cnt);
#endif // USE_TLB
            interrupt->Halt();
            break; // never reached
        
          case SC_Exit:
            DEBUG('a', "In Syscall Exit.\n");
            arg1 = machine->ReadRegister(4);
            printf("User program (tid=%d) exits with code: %d\n", 
                    currentThread->getThreadID(), arg1);
            currentThread->Finish();
            break; // never reached

          case SC_Create:
            DEBUG('a', "In Syscall Create.\n");

            arg1 = machine->ReadRegister(4);
            str = getStrArg(arg1);
            len = strlen(currentThread->space->currWorkDir) + strlen(str);
            filename = new char[len + 1];
            strcpy(filename, currentThread->space->currWorkDir);
            strcat(filename, str);

            if (!fileSystem->Create(filename, UNK)) {
                printf("Unable to create file \"%s\".\n", filename);
                ASSERT(FALSE);
            }
            delete[] str;
            delete[] filename;

            machine->UpdatePCinSyscall(); // increment the pc
            break;

          case SC_Open:
            DEBUG('a', "In Syscall Open.\n");

            arg1 = machine->ReadRegister(4);
            str = getStrArg(arg1);
            len = strlen(currentThread->space->currWorkDir) + strlen(str);
            filename = new char[len + 1];
            strcpy(filename, currentThread->space->currWorkDir);
            strcat(filename, str);

            openFile = fileSystem->Open(filename);
            delete[] str;
            delete[] filename;

            machine->WriteRegister(2, (int)openFile);
            machine->UpdatePCinSyscall(); // increment the pc
            break;

          case SC_Write:
            DEBUG('a', "In Syscall Write.\n");

            arg1 = machine->ReadRegister(4); // addr of data buffer in mem
            arg2 = machine->ReadRegister(5); // # of bytes
            arg3 = machine->ReadRegister(6); // OpenFileId
            buff = new char[arg2];
            ReadMemManyBytes(arg1, arg2, buff);

            if (arg3 == ConsoleInput) {
                printf("Cannot Write to ConsoleInput!\n");
                ASSERT(FALSE);
            } else if (arg3 == ConsoleOutput) {
                for (int i = 0; i < arg2; i++)
                    putchar(buff[i]);
            } else {
                openFile = (OpenFile *)arg3;
                openFile->Write(buff, arg2);
            }
            delete[] buff;

            machine->UpdatePCinSyscall(); // increment the pc
            break;

          case SC_Read: // TODO: console input & output
            DEBUG('a', "In Syscall Read.\n");

            arg1 = machine->ReadRegister(4); // addr of data buffer in mem
            arg2 = machine->ReadRegister(5); // # of bytes
            arg3 = machine->ReadRegister(6); // OpenFileId
            buff = new char[arg2];

            if (arg3 == ConsoleInput) {
                char ch;
                for (len = 0; len < arg2; len++) {
                    ch = getchar();
                    if (ch == '\n') { // end of input
                        break;
                    } else { // store in buff
                        buff[len] = ch;
                    }
                }
            } else if (arg3 == ConsoleOutput) {
                printf("Cannot Read from ConsoleOutput!\n");
                ASSERT(FALSE);
            } else {
                openFile = (OpenFile *)arg3;
                len = openFile->Read(buff, arg2);
            }
            WriteMemManyBytes(arg1, len, buff);
            delete[] buff;

            machine->WriteRegister(2, len);
            machine->UpdatePCinSyscall(); // increment the pc
            break;

          case SC_Close:
            DEBUG('a', "In Syscall Close.\n");

            arg1 = machine->ReadRegister(4);
            openFile = (OpenFile *)arg1;
            delete openFile; // close file

            machine->UpdatePCinSyscall(); // increment the pc
            break;
            
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
