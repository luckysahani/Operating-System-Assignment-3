#include "syscall.h"
#include "synchop.h"
#define SEMKEY 19
#define CONDKEY 17
int
main()
{
    int *array = (int *)sys_ShmAllocate(10*sizeof(int));
    array[0]=1;
    int id = sys_SemGet(SEMKEY);
    int id1 = sys_SemGet(20);
    int x = sys_Fork();
    if(x == 0) {
        sys_SemOp(id, -1);
        sys_Sleep(1000);
        sys_SemOp(id1, -1);
        sys_PrintString("In the child thread at the moment\n");
        array[0]=10;
        sys_SemOp(id1, 1);
        sys_SemOp(id, 1);
    } 
    else {
    //    while(array[0]!=10); // busy-wait
        sys_Sleep(10000);
        sys_SemOp(id1, -1);
        sys_SemOp(id, -1);
        sys_PrintString("In the parent thread at them moment");
        sys_SemOp(id, 1);
        sys_SemOp(id1,1);
    }
}