/* sort.c 
 *    Test program to sort a large number of integers.
 *
 *    Intention is to stress virtual memory system.
 *
 *    Ideally, we could read the unsorted array off of the file system,
 *	and store the result back to the file system!
 */

#include "syscall.h"
// #define NUM_ELEMS_A 1024 // size of physical memory; with code, we'll run out of space!
#define NUM_ELEMS_A 50

int A[NUM_ELEMS_A];	

int
main()
{
    int i, j, tmp;

    // first initialize the array, in reverse sorted order
    for (i = 0; i < NUM_ELEMS_A; i++)		
        A[i] = NUM_ELEMS_A - 1 - i;

    // then sort! 
    for (i = 0; i < NUM_ELEMS_A - 1; i++){
        for (j = 0; j < (NUM_ELEMS_A - 1 - i); j++){
	        if (A[j] > A[j + 1]) {	// out of order -> need to swap !
                tmp = A[j];
                A[j] = A[j + 1];
                A[j + 1] = tmp;
            }
        }
    }
        
    Exit(A[0]); // and then we're done -- should be 0!
}
