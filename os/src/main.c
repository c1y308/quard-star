#include <timeros/os.h>
void os_main()
{
   printk("hello timer os!\n");

   // 内存分配器初始化
   frame_alloctor_init();

   kvminit();

   
   kvminithart();

   trap_init();


   

   task_init();

   timer_init();

   
   // run_first_task();

}