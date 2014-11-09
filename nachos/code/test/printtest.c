#include "syscall.h"

int
main()
{
sys_PrintString("hello world\n");
sys_PrintString("Executed ");
sys_PrintInt(sys_GetNumInstr());
sys_PrintString(" instructions.\n");
return 0;
}