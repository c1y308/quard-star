#include <timeros/os.h>
#define CLOCK_FREQ 10000000 
#define TICKS_PER_SEC 100


/* 设置下次时钟中断的cnt值 */
void set_next_trigger()
{
    sbi_set_timer(r_mtime() + CLOCK_FREQ / TICKS_PER_SEC);
}

/* 开启S模式下的时钟中断 */
void timer_init()
{
   /* 先设置下次触发时刻, 清除启动时可能已 pending 的时钟中断
      (QEMU 上电后 mtimecmp=0 而 mtime>0, 使能 STIE 的瞬间会立即触发),
      避免在 stvec 仍指向 trap_from_kernel 时进入内核 trap 而 panic */
   set_next_trigger();
   reg_t sie = r_sie();
   sie |= SIE_STIE;
   w_sie(sie);
   reg_t sstatus =r_sstatus();
   sstatus |= (1L << 1) ;
   w_sstatus(sstatus);
}


/* 以us为单位返回时间 */
uint64_t get_time_us()
{
    reg_t time =  r_mtime() / (CLOCK_FREQ / TICKS_PER_SEC);
    return time;
}