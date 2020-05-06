/* userprog_syscall_test.c
 *	Simple program to test syscall Exec
 *
 * 	NOTE: for some reason, user programs with global data structures 
 *	sometimes haven't worked in the Nachos environment.  So be careful
 *	out there!  One option is to allocate data structures as 
 * 	automatics within a procedure, but if you do this, you have to
 *	be careful to allocate a big enough stack to hold the automatics!
 */

#include "syscall.h"

char exec_filename[10] = "matmult";
char create_filename[10] = "dummy";
char msg1[50] = "This is main thread!\n";
char msg2[50] = "This is forked thread!\n";
char msg3[60] = "File \"dummy\" does not yet exist, so main thread yields.\n";
int flag = 0;

void
SetFlag()
{
    flag = 1;
}

int
main()
{
    SpaceId spid;
    OpenFileId fd;
    int status;

    Fork(SetFlag);
    if (flag == 0) { // main thread
        Write(msg1, 22, ConsoleOutput);

        while (1) {
            fd = Open(create_filename);
            if (fd == 0) {
                Write(msg3, 57, ConsoleOutput);
                Yield();
            }
            else
                break;
        }
        Close(fd);

        spid = Exec(exec_filename);
        status = Join(spid);
        Exit(status);
    } else { // forked thread
        Write(msg2, 24, ConsoleOutput);
        Create(create_filename);
        Exit(0);
    }
}
