// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//  "_tid" is the thread which will run this user prog
//----------------------------------------------------------------------
AddrSpace::AddrSpace(OpenFile *executable, int _tid)
{
    printf("Initializing AddrSpace of tid %d\n", _tid);

    NoffHeader noffH;
    int i;
    unsigned int size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;
    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);

// set up the translation, and copy the code and data segments into memory
#ifdef INV_PG // use global inverted page table, thus support VM.
    // allocate a resident set for this user prog.
    // there must be enough page frames since MaxNumThreads 
    // is set as (NumPhysPages / ResSize).
    int tmp = 0;
    for (i = 0; i < NumPhysPages; i++) {
        if (machine->invPageTable[i].tid == -1) {
            ASSERT(machine->invPageTable[i].valid == FALSE);
            machine->invPageTable[i].tid = _tid;
            tmp++;
            if (tmp == ResSize) break;
        }
    }
    ASSERT(tmp == ResSize);

    // creat and open a swap file
    char swapFileName[10] = "swap_";
    sprintf(&swapFileName[5], "%d", _tid);
    //itoa(_tid, &swapFileName[5], 10);
    fileSystem->Create(swapFileName, size);

    machine->swapFiles[_tid] = fileSystem->Open(swapFileName);
    machine->ro_bmp[_tid] = new BitMap(numPages);

    // copy the contents of executable into swap file
    char buff[PageSize]; // copy one page per one time
    if (noffH.code.size > 0) {
        int sz = noffH.code.size, // remaining bytes in file to be copied
            pos = noffH.code.inFileAddr; // position within the file
        unsigned int vpn = (unsigned) noffH.code.virtualAddr / PageSize,
                     offset = (unsigned) noffH.code.virtualAddr % PageSize;
        while (sz > 0) {
            int block_size = min(PageSize - offset, sz);
            executable->ReadAt(buff, block_size, pos);
            machine->swapFiles[_tid]->WriteAt(
                        buff, block_size, vpn * PageSize + offset);
            if (block_size == PageSize) {       // if this page contains only codes,
                machine->ro_bmp[_tid]->Mark(vpn); // we could set it to be read-only.
            }
            sz -= block_size;
            pos += block_size;
            vpn++;
            offset = 0;
        }
    }
    if (noffH.initData.size > 0) {
        int sz = noffH.initData.size, // remaining bytes in file to be read
            pos = noffH.initData.inFileAddr; // position within the file
        unsigned int vpn = (unsigned) noffH.initData.virtualAddr / PageSize,
                     offset = (unsigned) noffH.initData.virtualAddr % PageSize;
        while (sz > 0) {
            int block_size = min(PageSize - offset, sz);
            executable->ReadAt(buff, block_size, pos);
            machine->swapFiles[_tid]->WriteAt(
                        buff, block_size, vpn * PageSize + offset);
            sz -= block_size;
            pos += block_size;
            vpn++;
            offset = 0;
        }
    }
    if (noffH.uninitData.size > 0) {
        int sz = noffH.uninitData.size, // remaining bytes in file to be read
            pos = noffH.uninitData.inFileAddr; // position within the file
        unsigned int vpn = (unsigned) noffH.uninitData.virtualAddr / PageSize,
                     offset = (unsigned) noffH.uninitData.virtualAddr % PageSize;
        while (sz > 0) {
            int block_size = min(PageSize - offset, sz);
            executable->ReadAt(buff, block_size, pos);
            machine->swapFiles[_tid]->WriteAt(
                        buff, block_size, vpn * PageSize + offset);
            sz -= block_size;
            pos += block_size;
            vpn++;
            offset = 0;
        }
    }

    // set page repl-algo-dependent variables
#ifdef PG_FIFO
	// TODO
#else // PG_LRU
	for (int i = 0; i < ResSize; i++)
        pg_lru[i] = -1;
#endif // PG_FIFO

#else // use normal page table, one per user prog. do not support VM.
    ASSERT(numPages <= machine->mem_bmp->NumClear()); // make sure there is 
                                    // enough memory for this user prog

    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;
        int ppn = machine->mem_bmp->Find();
        ASSERT(ppn != -1);
        pageTable[i].physicalPage = ppn;
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;
        bzero(&machine->mainMemory[ppn * PageSize], PageSize);
                // zero out the address space, to zero the unitialized 
                // data segment and the stack segment
    }

    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        int sz = noffH.code.size, // remaining bytes in file to be read
            pos = noffH.code.inFileAddr; // position within the file
        unsigned int vpn = (unsigned) noffH.code.virtualAddr / PageSize,
                     offset = (unsigned) noffH.code.virtualAddr % PageSize;
        while (sz > 0) {
            int block_size = min(PageSize - offset, sz);
            executable->ReadAt(
                &(machine->mainMemory[pageTable[vpn].physicalPage * PageSize + offset]),
			    block_size, pos);
            if (block_size == PageSize) {       // if this page contains only codes,
                pageTable[vpn].readOnly = TRUE; // we could set it to be read-only.
            }
            sz -= block_size;
            pos += block_size;
            vpn++;
            offset = 0;
        }
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        int sz = noffH.initData.size, // remaining bytes in file to be read
            pos = noffH.initData.inFileAddr; // position within the file
        unsigned int vpn = (unsigned) noffH.initData.virtualAddr / PageSize,
                     offset = (unsigned) noffH.initData.virtualAddr % PageSize;
        while (sz > 0) {
            int block_size = min(PageSize - offset, sz);
            executable->ReadAt(
                &(machine->mainMemory[pageTable[vpn].physicalPage * PageSize + offset]),
			    block_size, pos);
            sz -= block_size;
            pos += block_size;
            vpn++;
            offset = 0;
        }
    }
#endif // INV_PG

    // debug msg
    machine->mem_bmp->Print();

#ifdef USE_TLB
    tlb_lookup_cnt = 0;
    tlb_miss_cnt = 0;
#endif // USE_TLB

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    int i;

// clear memory bitmap
#ifdef INV_PG // use global inverted page table, thus support VM.
    int _tid = currentThread->getThreadID();
    for (i = 0; i < NumPhysPages; i++) {
        if (machine->invPageTable[i].tid == _tid && 
                machine->invPageTable[i].valid) {
            machine->mem_bmp->Clear(i);
            machine->invPageTable[i].valid = FALSE;
            machine->invPageTable[i].tid = -1;
        }
    }

    // close and remove the swap file
    
    delete machine->swapFiles[_tid];
    machine->swapFiles[_tid] = NULL;
    delete machine->ro_bmp[_tid];
    machine->ro_bmp[_tid] = NULL;

    char swapFileName[10] = "swap_";
    sprintf(&swapFileName[5], "%d", _tid);
    //itoa(_tid, &swapFileName[5], 10);
    fileSystem->Remove(swapFileName);

#else // use normal page table, one per user prog. do not support VM.
    for (i = 0; i < numPages; i++) {
        if (pageTable[i].valid)
            machine->mem_bmp->Clear(pageTable[i].physicalPage);
    }
    delete [] pageTable;
#endif // INV_PG
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
#ifndef INV_PG // use normal page table, one per user prog. do not support VM.
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
#endif // INV_PG

#ifdef USE_TLB
    for (int i = 0; i < TLBSize; i++) // flush TLB
        machine->tlb[i].valid = false;

    // reset TLB repl-algo-dependent variables
#ifdef TLB_FIFO
	machine->tlb_next_repl = 0;
#else // TLB_LRU
	for (int i = 0; i < TLBSize; i++)
        machine->tlb_lru[i] = -1;
#endif // TLB_FIFO

#endif // USE_TLB
}
