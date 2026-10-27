#include <timeros/os.h>

void os_main()
{
   intr_off();
   printk("hello timer os!\n");

   // 内存分配器初始化
   frame_alloctor_init();
   
   //初始化内存
   kvminit();

   //初始化磁盘
   virtio_disk_init();
   binit();

   //初始化进程控制块
   procinit();

   //映射内核
   kvminithart();

   //trap初始化
   set_kernel_trap_entry();
   timer_init();
   device_init();

   // 设备中断就绪后再执行磁盘 I/O。
   fs_init();
   load_initproc();
   app_init();

   run_first_task();

}
