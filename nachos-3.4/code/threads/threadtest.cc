// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "elevatortest.h"

// testnum is set in main.cc
int testnum = 1;

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int dummy)
{
    int num;
    
    for (num = 0; num < 5; num++) {
	printf("*** thread \"%s\" (tid=%d) looped %d times\n", 
            currentThread->getName(), currentThread->getThreadID(), 
            num);
        // ThreadsStatus();
         scheduler->Print();
        currentThread->Yield();
    }
}

//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, (void*)0);
    SimpleThread(0);
}

//----------------------------------------------------------------------
// ThreadTest2
// 	Creat n new threads recursively,
//  and yield CPU to the thread which calls the next recursion level.
//----------------------------------------------------------------------

static int CalledTimes = 0;

void UpToCeiling(int ceiling){
    if(CalledTimes >= ceiling){
        printf("tid %d's play: Ceiling touched!\n", 
                currentThread->getThreadID());
        return;
    }
    printf("tid %d's play: Ceiling not touched.\n", 
            currentThread->getThreadID());

    CalledTimes++;
    Thread *t = new Thread("AnyName");
    t->Fork(UpToCeiling, (void*)(ceiling));

    while(CalledTimes < ceiling){
        printf("tid %d's play: Ceiling not touched.\n", 
                currentThread->getThreadID());
        currentThread->Yield();
    }
    printf("tid %d's play: Ceiling touched!\n", 
            currentThread->getThreadID());
    return;
}

void
ThreadTest2()
{
    DEBUG('t', "Entering ThreadTest2");
    
     UpToCeiling(5);
    // UpToCeiling(127);
    // UpToCeiling(128);
}

#ifdef SCHED_PRI_PRMPT

//----------------------------------------------------------------------
// ThreadTest3
// 	Almost same as ThreadTest1 except that this is to 
//  test SCHED_PRI_PRMPT
//----------------------------------------------------------------------

void
ThreadTest3()
{
    DEBUG('t', "Entering ThreadTest3");

    Thread *t;
    t = new Thread("pri3", 3);
    t->Fork(SimpleThread, (void*)0);
    t = new Thread("pri0", 0);
    t->Fork(SimpleThread, (void*)0);

    SimpleThread(0);
}
#endif // SCHED_PRI_PRMPT

#ifdef SCHED_RR

//----------------------------------------------------------------------
// ThreadTest4
// 	
//----------------------------------------------------------------------

static int timeToQuit = 10000;

void FakeSysCall(int dummy){
    int i = 0;
    while (stats->totalTicks < timeToQuit)
    {
        printf("*** thread \"%s\" looped %d times\n", 
            currentThread->getName(), i++);
        interrupt->SetLevel(IntOff);
        interrupt->SetLevel(IntOn);
    }
}

void
ThreadTest4()
{
    DEBUG('t', "Entering ThreadTest4");

    Thread *t;
    t = new Thread("forked1");
    t->Fork(FakeSysCall, (void*)0);
    t = new Thread("forked2");
    t->Fork(FakeSysCall, (void*)0);

    FakeSysCall(0);
}
#endif // SCHED_RR

//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 1:
	ThreadTest1();
    break;
    case 2:
	ThreadTest2();
	break;
#ifdef SCHED_PRI_PRMPT
    case 3:
	ThreadTest3();
	break;
#endif
#ifdef SCHED_RR
    case 4:
	ThreadTest4();
	break;
#endif
    default:
	printf("No test specified.\n");
	break;
    }
}

