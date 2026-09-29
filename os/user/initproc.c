#include <timeros/types.h>
#include <timeros/syscall.h>
#include <timeros/string.h>
int main()
{
    /* 0 号进程(initproc)直接 exec 成用户 shell */
    sys_exec("user_shell");
    return 0;
}
