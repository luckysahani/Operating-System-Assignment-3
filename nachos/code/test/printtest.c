/* printtest.c
 *	Simple program to test whether printing from a user program works.
 *	
 *	Just do a "syscall" that shuts down the OS.
 *
 * 	NOTE: for some reason, user programs with global data structures 
 *	sometimes haven't worked in the Nachos environment.  So be careful
 *	out there!  One option is to allocate data structures as 
 * 	automatics within a procedure, but if you do this, you have to
 *	be careful to allocate a big enough stack to hold the automatics!
 */

#include "syscall.h"

int
main()
{
    // sys_PrintString("hello world\n");
    // sys_PrintString("Executed ");
    // sys_PrintInt(sys_GetNumInstr());
    // sys_PrintString(" instructions.\n");

    int *array = (int*)sys_ShmAllocate(2*sizeof(int));
 //   array[0]=1;
    int x = sys_Fork();
    if (x == 0) {
    	array[0]=1;
    }
    else{
    	sys_Sleep(1000);
    	sys_PrintString("hello \n");
    	sys_PrintInt(array[0]);
    }
    return 0;
}
