/* file_syscall_test.c
 *	Simple program to test Create()
 *
 * 	NOTE: for some reason, user programs with global data structures 
 *	sometimes haven't worked in the Nachos environment.  So be careful
 *	out there!  One option is to allocate data structures as 
 * 	automatics within a procedure, but if you do this, you have to
 *	be careful to allocate a big enough stack to hold the automatics!
 */

#include "syscall.h"

char filename[10] = "myfile";

int
main()
{
    Create(filename);
    return 0;
}
