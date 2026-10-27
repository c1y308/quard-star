#include <timeros/os.h>

void trap_from_kernel(void)
{
    reg_t scause = r_scause();
    if (scause & (1UL << 63)) {
        switch (scause & 0xfff) {
        case 5:
            set_next_trigger();
            return;
        case 9:
            irq_handler();
            return;
        }
    }
    printk("unexpected kernel trap scause:%lx sepc:%lx stval:%lx\n",
           scause, r_sepc(), r_stval());
    panic("unexpected kernel trap");
}

void set_kernel_trap_entry()
{
	w_stvec((reg_t)kernel_trap_entry);
}

void set_user_trap_entry()
{
	w_stvec((reg_t)TRAMPOLINE);  
}

void trap_handler()
{
    set_kernel_trap_entry();
    TrapContext* cx = (TrapContext*)get_current_trap_cx();
    reg_t scause = r_scause();
    reg_t cause_code = scause & 0xfff;

    if (scause & 0x8000000000000000ULL) {
        switch (cause_code) {
        case 5:
            set_next_trigger();
            schedule();
            break;
        case 9:
            irq_handler();
            break;
        default:
            printk("undefined interrupt scause:%lx\n", scause);
            panic("unexpected user interrupt");
        }
    } else {
        switch (cause_code) {
        case 8: {
            /* exec 会覆写 sepc，必须先跳过 4 字节的 ecall 指令。 */
            cx->sepc += 4;
            uint64_t result = __SYSCALL(cx->a7, cx->a0, cx->a1, cx->a2);
            /* 调度可能切换当前进程，返回值写入当前 trap 上下文。 */
            cx = (TrapContext*)get_current_trap_cx();
            cx->a0 = result;
            break;
        }
        default:
            printk("undefined exception scause:%lx sepc:%lx stval:%lx\n",
                   scause, cx->sepc, r_stval());
            panic("unexpected user exception");
        }
    }
    trap_return();
}

void trap_return()  
{  
    /* 把 stvec 设置为内核和应用地址空间共享的跳板页面的起始地址 */  
    set_user_trap_entry();
    /* Trap 上下文在应用地址空间中的虚拟地址 */  
    u64 trap_cx_ptr = current_thread()->trap_cx_va;
    /* 要继续执行的应用地址空间的 token */  
    u64  user_satp = current_thread_user_token();
	/* 计算_restore函数的虚拟地址 */
    u64  restore_va = (u64)__restore - (u64)__alltraps + TRAMPOLINE;  
	asm volatile (    
			"fence.i\n\t"    
			"mv a0, %0\n\t"  // 将trap_cx_ptr传递给a0寄存器  
			"mv a1, %1\n\t"  // 将user_satp传递给a1寄存器  
			"jr %2\n\t"      // 跳转到restore_va的位置执行代码  
			:    
			: "r" (trap_cx_ptr),    
			"r" (user_satp),
			"r" (restore_va)
			: "a0", "a1"
		);
  
}
