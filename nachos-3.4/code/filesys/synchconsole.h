// synchconsole.h 
// 	Data structures to export a synchronous interface to the console device.

#ifndef SYNCHCONSOLE_H
#define SYNCHCONSOLE_H

#include "console.h"
#include "synch.h"

// The following class defines a "synchronous" console abstraction.
class SynchConsole {
  public:
    SynchConsole(char *readFile, char *writeFile); // Initialize a synch console,
					                        // by initializing the raw console.
    ~SynchConsole(); // De-allocate the synch console data

// external interface -- Nachos kernel code can call these.
// These call Console::PutChar/GetChar and then wait until the request is done.
    void PutChar(char ch); // Write "ch" to the console display, returning
    					// only once the character is actually writen
    char GetChar(); // Poll the console input, returning only once a char is available.

// internal emulation routines -- DO NOT call these.
    void SynchWriteDone(); // Called by the console interrupt handler, to 
                        // signal that the character is actually writen
    void SynchReadDone(); // Called by the console interrupt handler, to 
                        // signal that a character is avail to be read

  private:
    Console *cons; // Raw console
    Semaphore *read_sem; // To synchronize read
    Semaphore *write_sem; // To synchronize write
    Lock *rlock; // Only one read request can be sent to the console at a time
    Lock *wlock; // Only one write request can be sent to the console at a time
};

#endif // SYNCHCONSOLE_H