#include "syscall.h"
#include "synchop.h"

int main()
{
int x;
int sleep_start, sleep_end;
int *array = (int*)sys_ShmAllocate(2*sizeof(int));
// sys_PrintString("Parent PID: ");
// sys_PrintChar('\n');

// int y = sys_SemGet(1);
// int t = sys_CondGet(1);
array[0] = 5;
array[1] = 0;
int a = 1;
// sys_SemCtl(y, SYNCH_SET, &a);
x = sys_Fork();
if (x == 0) {
// sys_SemOp(y,-1);
// sys_CondOp(t,COND_OP_WAIT, y);
array[0] = 1;
sys_PrintInt(array[1]);
sys_PrintChar('\n');
sys_PrintString("Finish Chils");
// sys_SemOp(y,1);
}
else {
// sys_SemOp(y,-1);
array[1] = 1;
sys_PrintInt(array[0]);
sys_PrintChar('\n');
// sys_CondOp(t,COND_OP_SIGNAL, y);
// sys_SemOp(y,1);
sys_Join(x);
sys_PrintString("Finish Parent");
}
sys_PrintString("Finish");
return 0;
}