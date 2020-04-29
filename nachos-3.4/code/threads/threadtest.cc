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
#include "synch.h"

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
// ThreadTest5
// 	 Solve the Producer&Consumer problem using condition variable.
//   Run Nachos with "-rs" to allow random switches between producer 
//   threads and consumer threads.
//----------------------------------------------------------------------

Condition cond_full("cond_full"), // producers wait on it
        cond_empty("cond_empty"); // consumers wait on it
Lock lock_pc("lock_pc");
int num_product = 0; // number of products
const int max_num_product = 15;

// Produce 'turns' number of products, one at a time.
void Produce_inTest5(int turns) {
    while (turns--)
    {
        ASSERT(num_product >= 0 && num_product <= max_num_product);
        lock_pc.Acquire();	// enforce mutual exclusive access to the products

        while (num_product == max_num_product) {
            printf("#product = %d, full, \"%s\" go to sleep.\n",
                    num_product, currentThread->getName());
	        cond_full.Wait(&lock_pc); // wait until not full
        }
        num_product++;
        printf("#product = %d, \"%s\" just produced one product.\n",
                    num_product, currentThread->getName());

        cond_empty.Signal(&lock_pc); // wake up a consumer, if any
        // cond_empty.Broadcast(&lock_pc); // wake up all consumers, if any

        lock_pc.Release();
    }
}

// Consume 'turns' number of products, one at a time.
void Consume_inTest5(int turns) {
    while (turns--)
    {
        ASSERT(num_product >= 0 && num_product <= max_num_product);
        lock_pc.Acquire();	// enforce mutual exclusive access to the products

        while (num_product == 0) {
            printf("#product = %d, empty, \"%s\" go to sleep.\n",
                    num_product, currentThread->getName());
	        cond_empty.Wait(&lock_pc); // wait until not empty
        }
        num_product--;
        printf("#product = %d, \"%s\" just consumed one product.\n",
                    num_product, currentThread->getName());

        cond_full.Signal(&lock_pc); // wake up a producer, if any
        // cond_full.Broadcast(&lock_pc); // wake up all producers, if any

        lock_pc.Release();
    }
}

void ThreadTest5() {
    DEBUG('t', "Entering ThreadTest5");

    Thread *p1 = new Thread("p1"), *p2 = new Thread("p2"),
           *c1 = new Thread("c1"), *c2 = new Thread("c2");
    p1->Fork(Produce_inTest5, 20);
    p2->Fork(Produce_inTest5, 20);
    c1->Fork(Consume_inTest5, 20);
    c2->Fork(Consume_inTest5, 20);
}

//----------------------------------------------------------------------
// ThreadTest6
// 	 Solve the Producer&Consumer problem using semaphore.
//   Run Nachos with "-rs" to allow random switches between producer 
//   threads and consumer threads.
//----------------------------------------------------------------------

Semaphore sem_full("sem_full", 0), 
            // number of products yet to be consumed at this moment
        sem_empty("sem_empty", max_num_product); 
            // number of new products can be made at this moment

// Produce 'turns' number of products, one at a time.
void Produce_inTest6(int turns) {
    while (turns--)
    {
        ASSERT(num_product >= 0 && num_product <= max_num_product);
        sem_empty.P();
        lock_pc.Acquire();	// enforce mutual exclusive access to the products

        num_product++;
        printf("#product = %d, \"%s\" just produced one product.\n",
                    num_product, currentThread->getName());

        lock_pc.Release();
        sem_full.V();
    }
}

// Consume 'turns' number of products, one at a time.
void Consume_inTest6(int turns) {
    while (turns--)
    {
        ASSERT(num_product >= 0 && num_product <= max_num_product);
        sem_full.P();
        lock_pc.Acquire();	// enforce mutual exclusive access to the products

        num_product--;
        printf("#product = %d, \"%s\" just consumed one product.\n",
                    num_product, currentThread->getName());

        lock_pc.Release();
        sem_empty.V();
    }
}

void ThreadTest6() {
    DEBUG('t', "Entering ThreadTest6");

    Thread *p1 = new Thread("p1"), *p2 = new Thread("p2"),
           *c1 = new Thread("c1"), *c2 = new Thread("c2");
    p1->Fork(Produce_inTest6, 20);
    p2->Fork(Produce_inTest6, 20);
    c1->Fork(Consume_inTest6, 20);
    c2->Fork(Consume_inTest6, 20);
}


//----------------------------------------------------------------------
// ThreadTest7
// 	 Implement barrier using condition variable.
//----------------------------------------------------------------------

Condition cond_barr("cond_barr");
Lock lock_barr("lock_barr");
int num_threads_reached = 0; // number of threads that have printed names

void BarrierTest(int dummy) {
    printf("%s\n", currentThread->getName());

    // ---------This line is the barrier---------
    lock_barr.Acquire();	// enforce mutual exclusive access to the products
    num_threads_reached++;
    while (num_threads_reached < 7) {
        cond_barr.Wait(&lock_barr); // wait until all threads reach barrier
    }
    cond_barr.Broadcast(&lock_barr); // wake up all threads
    lock_barr.Release();
    // ---------End of the barrier---------

    printf("我们是金刚葫芦娃！\n");
}

void ThreadTest7() {
    DEBUG('t', "Entering ThreadTest7");

    Thread *t1 = new Thread("我是大娃，我力大无穷，身体可以变大！"), 
           *t2 = new Thread("我是二娃，我有千里眼、顺风耳！"),
           *t3 = new Thread("我是三娃，我有铜头铁臂，刀枪不入！"), 
           *t4 = new Thread("我是四娃，我会吐火，把妖怪烧个精光！"),
           *t5 = new Thread("我是五娃，我会喷水，妖怪无处可躲！"),
           *t6 = new Thread("我是六娃，我会隐身术，来无影去无踪！"),
           *t7 = new Thread("我是七娃，我有宝葫芦，可以吸妖怪！");
    t1->Fork(BarrierTest, 0);
    t2->Fork(BarrierTest, 0);
    t3->Fork(BarrierTest, 0);
    t4->Fork(BarrierTest, 0);
    t5->Fork(BarrierTest, 0);
    t6->Fork(BarrierTest, 0);
    t7->Fork(BarrierTest, 0);
}

//----------------------------------------------------------------------
// ThreadTest8
// 	 Test synchronized read/write to the same file among multiple threads.
//   Also test falsy removal of a file which is still opened elsewhere.
//----------------------------------------------------------------------
#ifdef FILESYS

static Semaphore sem("sem", 0);

void ReadAndRemoveFile (int dummy) {
    OpenFile *openFile = fileSystem->Open("/threadtest8.txt");

    char str[11];
    openFile->Read(str, 10);
    str[10] = '\0';
    printf("*** thread \"%s\" successfully read file: %s\n", currentThread->getName(), str);

    delete openFile; // close file

    printf("*** thread \"%s\" attempts to remove file.\n", currentThread->getName());
    if (fileSystem->Remove("/threadtest8.txt") == TRUE)
        printf("*** thread \"%s\" successfully remove file.\n", currentThread->getName());
    
    sem.V();
}

void WriteAndRemoveFile (int dummy) {
    OpenFile *openFile = fileSystem->Open("/threadtest8.txt");

    openFile->Write("0123456789", 10);
    printf("*** thread \"%s\" successfully write file.\n", currentThread->getName());

    delete openFile; // close file

    printf("*** thread \"%s\" attempts to remove file.\n", currentThread->getName());
    if (fileSystem->Remove("/threadtest8.txt") == TRUE)
        printf("*** thread \"%s\" successfully remove file.\n", currentThread->getName());

    sem.V();
}

void ThreadTest8() {
    DEBUG('t', "Entering ThreadTest8");

    fileSystem->Create("/threadtest8.txt", TXT);
    OpenFile *openFile = fileSystem->Open("/threadtest8.txt");

    openFile->Write("0000000000", 10);

    Thread *t1 = new Thread("forked1"), 
           *t2 = new Thread("forked2");
    t1->Fork(WriteAndRemoveFile, 0);
    t2->Fork(ReadAndRemoveFile, 0);

    currentThread->Yield();
    sem.P();
    sem.P();

    delete openFile; // close file

    printf("*** thread \"%s\" attempts to remove file.\n", currentThread->getName());
    if (fileSystem->Remove("/threadtest8.txt") == TRUE)
        printf("*** thread \"%s\" successfully remove file.\n", currentThread->getName());
}
#endif // FILESYS

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
    case 5:
	ThreadTest5();
	break;
    case 6:
	ThreadTest6();
	break;
    case 7:
	ThreadTest7();
	break;
#ifdef FILESYS
    case 8:
	ThreadTest8();
	break;
#endif // FILESYS
    default:
	printf("No test specified.\n");
	break;
    }
}

