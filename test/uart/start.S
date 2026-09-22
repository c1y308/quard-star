/*
 * quard-star UART 测试固件的复位入口
 *
 * CPU 复位后从 MROM(0x0) 的复位向量跳转到 Flash 基址 0x20000000 执行本代码
 * （见 quard_star.c 的 riscv_setup_rom_reset_vec）。此时处于 M 模式，
 * 可直接 MMIO 访问 0x10000000 的 UART，无需任何 PMP/PMA 配置。
 */
    .section .text.init
    .globl _start
    .type  _start, @function
_start:
    /* 多核时只让 hart0 打印，其余核原地停机，避免输出交错 */
    csrr    a0, mhartid
    bnez    a0, .Lpark

    /* 在 DRAM(0x80000000) 里给 C 代码准备一段栈：栈顶 = 0x80100000 */
    li      sp, 0x80100000

    /* 进入 C 主逻辑（不返回） */
    call    uart_main

.Lpark:
    wfi
    j       .Lpark
    .size   _start, . - _start
