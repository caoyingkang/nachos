// main.cc 
//	Bootstrap code to initialize the operating system kernel.
//
//	Allows direct calls into internal operating system functions,
//	to simplify debugging and testing.  In practice, the
//	bootstrap code would just initialize data structures,
//	and start a user program to print the login prompt.
//
// 	Most of this file is not needed until later assignments.
//
// Usage: nachos -d <debugflags> -rs <random seed #>
//		-s -x <nachos file> -c <consoleIn> <consoleOut>
//		-f -cp <unix file> <nachos file>
//		-p <nachos file> -r <nachos file> -l -D -t
//              -n <network reliability> -m <machine id>
//              -o <other machine id>
//              -z
//
//    -d causes certain debugging messages to be printed (cf. utility.h)
//    -rs causes Yield to occur at random (but repeatable) spots
//    -z prints the copyright message
//
//  USER_PROGRAM
//    -s causes user programs to be executed in single-step mode
//    -x runs a user program
//    -c tests the console
//
//  FILESYS
//    -f causes the physical disk to be formatted
//    -cp copies a file from UNIX to Nachos
//    -p prints a Nachos file to stdout
//    -r removes a Nachos file from the file system
//    -l lists the contents of the Nachos directory
//    -D prints the contents of the entire file system 
//    -t tests the performance of the Nachos file system
//
//  NETWORK
//    -n sets the network reliability
//    -m sets this machine's host id (needed for the network)
//    -o runs a simple test of the Nachos network software
//
//  NOTE -- flags are ignored until the relevant assignment.
//  Some of the flags are interpreted here; some in system.cc.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#define MAIN
#include "copyright.h"
#undef MAIN

#include "utility.h"
#include "system.h"

#ifdef THREADS
extern int testnum;
#endif

// External functions used by this file

extern void ThreadTest(void), Copy(char *unixFile, char *nachosFile);
extern void Print(char *file), PerformanceTest(void);
extern void StartProcess(char *file), ConsoleTest(char *in, char *out);
extern void MailTest(int networkID);
extern void MakeDir(char *name);

//----------------------------------------------------------------------
// main
// 	Bootstrap the operating system kernel.  
//	
//	Check command line arguments
//	Initialize data structures
//	(optionally) Call test procedure
//
//	"argc" is the number of command line arguments (including the name
//		of the command) -- ex: "nachos -d +" -> argc = 3 
//	"argv" is an array of strings, one for each command line argument
//		ex: "nachos -d +" -> argv = {"nachos", "-d", "+"}
//----------------------------------------------------------------------

#ifdef USER_PROGRAM
/////////////////////////////////////////////
// test multiprogramming without SC_Exec
void StartSortProg(int dummy) {
	StartProcess("sort");
}
/////////////////////////////////////////////
#endif // USER_PROGRAM

int
main(int argc, char **argv)
{
	int _argc;
	char **_argv;
    int argCount; // the number of arguments for a particular command

    DEBUG('t', "Entering main");
    (void) Initialize(argc, argv);
    
#ifdef THREADS
	_argc = argc;
	_argv = argv;
    for (_argc--, _argv++; _argc > 0; _argc -= argCount, _argv += argCount) {
		argCount = 1;
		switch (_argv[0][1]) {
		case 'q':
			testnum = atoi(_argv[1]);
			argCount++;
			break;
		default:
			testnum = -1;
			break;
		}
    }

	if (testnum >= 1)
    	ThreadTest();
#endif

	_argc = argc;
	_argv = argv;
    for (_argc--, _argv++; _argc > 0; _argc -= argCount, _argv += argCount) {
		argCount = 1;
        if (!strcmp(*_argv, "-z")) // print copyright
            printf (copyright);
#ifdef USER_PROGRAM
        if (!strcmp(*_argv, "-x")) {        	// run a user program
	    	ASSERT(_argc > 1);

/////////////////////////////////////////////
// test multiprogramming without SC_Exec
			Thread *t = new Thread("forked");
			t->Fork(StartSortProg, 0);
/////////////////////////////////////////////

            StartProcess(*(_argv + 1));
            argCount = 2;
        } else if (!strcmp(*_argv, "-c")) { // test the console
			if (_argc == 1)
				ConsoleTest(NULL, NULL);
			else {
				ASSERT(_argc > 2);
				ConsoleTest(*(_argv + 1), *(_argv + 2));
				argCount = 3;
			}
			interrupt->Halt(); // once we start the console, then 
						// Nachos will loop forever waiting 
						// for console input
		} else if (!strcmp(*_argv, "-sc")) { // test the synch console
			if (_argc == 1)
				ConsoleTest(NULL, NULL);
			else {
				ASSERT(_argc > 2);
				ConsoleTest(*(_argv + 1), *(_argv + 2));
				argCount = 3;
			}
			interrupt->Halt(); // once we start the console, then 
						// Nachos will loop forever waiting 
						// for console input
		}
#endif // USER_PROGRAM
#ifdef FILESYS
		if (!strcmp(*_argv, "-cp")) { 		// copy from UNIX to Nachos
			ASSERT(_argc > 2);
			Copy(*(_argv + 1), *(_argv + 2));
			argCount = 3;
		} else if (!strcmp(*_argv, "-p")) {	// print a Nachos file
			ASSERT(_argc > 1);
			Print(*(_argv + 1));
			argCount = 2;
		} else if (!strcmp(*_argv, "-r")) {	// remove Nachos file
			ASSERT(_argc > 1);
			fileSystem->Remove(*(_argv + 1));
			argCount = 2;
		} else if (!strcmp(*_argv, "-l")) {	// list Nachos directory
			fileSystem->List();
		} else if (!strcmp(*_argv, "-D")) {	// print entire filesystem
			fileSystem->Print();
		} else if (!strcmp(*_argv, "-t")) {	// performance test
			PerformanceTest();
		} else if (!strcmp(*_argv, "-mkdir")) { // creat a new dir
			ASSERT(_argc > 1);
			MakeDir(*(_argv + 1));
			argCount = 2;
		}
#endif // FILESYS
#ifdef NETWORK
        if (!strcmp(*_argv, "-o")) {
	    	ASSERT(_argc > 1);
            Delay(2); 	// delay for 2 seconds
						// to give the user time to 
						// start up another nachos
            MailTest(atoi(*(_argv + 1)));
            argCount = 2;
        }
#endif // NETWORK
    }

    currentThread->Finish();	// NOTE: if the procedure "main" 
				// returns, then the program "nachos"
				// will exit (as any other normal program
				// would).  But there may be other
				// threads on the ready list.  We switch
				// to those threads by saying that the
				// "main" thread is finished, preventing
				// it from returning.
    return(0);			// Not reached...
}
