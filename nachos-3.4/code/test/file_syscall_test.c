/* file_syscall_test.c
 *	Simple program to test syscall Create, Open, Close, Read, Write
 *
 * 	NOTE: for some reason, user programs with global data structures 
 *	sometimes haven't worked in the Nachos environment.  So be careful
 *	out there!  One option is to allocate data structures as 
 * 	automatics within a procedure, but if you do this, you have to
 *	be careful to allocate a big enough stack to hold the automatics!
 */

#include "syscall.h"
#define MaxBuffSize 50

char filename[7] = "myfile";
char write_data[MaxBuffSize];
char read_data[MaxBuffSize];
char errmsg[19] = "Cannot open file!\n";
char newline = '\n';

int
main()
{
    int fd1, fd2;
    int write_bytes, read_bytes;

// input string
    write_bytes = Read(write_data, MaxBuffSize, ConsoleInput);

// create a new file
    Create(filename);

// open and write the file
    fd1 = Open(filename);
    if (fd1 == 0) {
        Write(errmsg, 19, ConsoleOutput);
        Exit(1);
    }
    Write(write_data, write_bytes, fd1);
    Write(write_data, write_bytes, fd1);

// open and read the file
    fd2 = Open(filename);
    if (fd2 == 0) {
        Write(errmsg, 19, ConsoleOutput);
        Exit(1);
    }
    read_bytes = Read(read_data, MaxBuffSize, fd2);

// print what was read, plus a newline character
    Write(read_data, read_bytes, ConsoleOutput);
    Write(&newline, 1, ConsoleOutput);

// close files and exit
    Close(fd1);
    Close(fd2);
    Exit(666);
    return 0;
}
