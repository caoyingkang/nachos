// synchconsole.cc 
//	Routines to synchronously use the console. The raw console is an 
//	asynchronous device (write requests return immediately, and an
//	interrupt happens later on; read requests return immediately, no 
//  matter if the char is avail, i.e. not EOF). This is a layer on top
//	of the raw console providing a synchronous interface (requests wait
//	until the request completes).

#include "synchconsole.h"

//----------------------------------------------------------------------
// DummySynchWriteDone, DummySynchReadDone
//	dummy function because C++ can't handle pointers to member functions.
//----------------------------------------------------------------------
static void DummySynchWriteDone (int sc)
{
    SynchConsole* syncons = (SynchConsole *)sc;
    syncons->SynchWriteDone();
}
static void DummySynchReadDone (int sc)
{
    SynchConsole* syncons = (SynchConsole *)sc;
    syncons->SynchReadDone();
}

//----------------------------------------------------------------------
// SynchConsole::SynchConsole
// 	Initialize the synchronous interface to the raw console.
//
//	"readFile" -- UNIX file simulating the keyboard (NULL -> use stdin)
//	"writeFile" -- UNIX file simulating the display (NULL -> use stdout)
//----------------------------------------------------------------------
SynchConsole::SynchConsole(char *readFile, char *writeFile)
{
    write_sem = new Semaphore("synch console write sem", 0);
    read_sem = new Semaphore("synch console read sem", 0);
    wlock = new Lock("synch console write lock");
    rlock = new Lock("synch console read lock");
    cons = new Console(readFile, writeFile, 
                    DummySynchReadDone, DummySynchWriteDone, (int)this);
}

//----------------------------------------------------------------------
// SynchConsole::~SynchConsole
// 	De-allocate data structures needed for the synchronous console
//	abstraction.
//----------------------------------------------------------------------
SynchConsole::~SynchConsole()
{
    delete wlock;
    delete rlock;
    delete write_sem;
    delete read_sem;
    delete cons;
}

//----------------------------------------------------------------------
// SynchConsole::PutChar()
//  Write "ch" to the console display, returning only once the character
//  is actually writen.
//----------------------------------------------------------------------
void
SynchConsole::PutChar(char ch)
{
    wlock->Acquire();
    cons->PutChar(ch);
    write_sem->P(); // wait for interrupt
    wlock->Release();
}

//----------------------------------------------------------------------
// SynchConsole::GetChar
// 	Poll the console input, returning only once a char is available.
//----------------------------------------------------------------------
char
SynchConsole::GetChar()
{
    char ch;
    rlock->Acquire();
    read_sem->P(); // wait for interrupt
    ch = cons->GetChar();
    ASSERT(ch != EOF);
    rlock->Release();
    return ch;
}

//----------------------------------------------------------------------
// SynchConsole::SynchWriteDone
// 	Console interrupt handler. Wake up any thread waiting for the PutChar
//	request to finish.
//----------------------------------------------------------------------
void
SynchConsole::SynchWriteDone()
{
    write_sem->V();
}

//----------------------------------------------------------------------
// SynchConsole::SynchReadDone
// 	Console interrupt handler. Wake up any thread waiting for a character
//	to be avail.
//----------------------------------------------------------------------
void
SynchConsole::SynchReadDone()
{
    read_sem->V();
}