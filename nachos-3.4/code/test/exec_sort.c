/* exec_sort.c 
 *    Test program to fork a new thread executing sort.
 *
 *    Intention is to force multiprogramming.
 */

#include "syscall.h"

int main()
{
    char name[5] = "sort";
    Exec(name);
        
    Exit(0);
}
