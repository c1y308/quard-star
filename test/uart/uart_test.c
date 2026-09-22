/*
 * quard-star 最小裸机 UART 打印测试固件
 *
 * 目的：绕过 OpenSBI / OS 的整条启动链，直接访问 QEMU 在 quard_star.c
 *       里用 serial_mm_init() 建模的 16550 兼容串口，验证板级 UART 模型
 *       与地址映射是否正确。
 *
 * UART0 物理基址 = 0x10000000（见 quard_star.c 的 quard_star_memmap 与
 *                  quard_star_serial_create），寄存器间隔 = 1 字节
 *                  （serial_mm_init 的 itshift = 0）。
 */
#include <stdint.h>

#define UART0_BASE  ((uintptr_t)0x10000000UL)

/* 16550 关键寄存器（itshift=0，偏移即字节地址） */
#define UART_THR    (*(volatile uint8_t *)(UART0_BASE + 0x0))  /* 发送保持寄存器(写) */
#define UART_LSR    (*(volatile uint8_t *)(UART0_BASE + 0x5))  /* 线状态寄存器(读)   */

#define LSR_THRE    0x20   /* bit5: Transmit Holding Register Empty，可发送 */

/* 向串口发送一个字节：先轮询等待发送寄存器空，再写入 THR */
static void uart_putchar(char c)
{
    while ((UART_LSR & LSR_THRE) == 0) {
        /* busy wait */
    }
    UART_THR = (uint8_t)c;
}

/* 发送字符串，并把 '\n' 补成 "\r\n" 便于终端正确换行 */
static void uart_puts(const char *s)
{
    while (*s != '\0') {
        if (*s == '\n') {
            uart_putchar('\r');
        }
        uart_putchar(*s);
        s++;
    }
}

/* 由 start.S 的 _start 调用（不返回） */
void uart_main(void)
{
    uart_puts("[uart-test] hello from quard-star baremetal!\n");
    uart_puts("[uart-test] 16550 UART0 @ 0x10000000 is working.\n");

    /* 打印 0..9，验证连续多字节发送与 THRE 轮询逻辑 */
    uart_puts("[uart-test] counting: ");
    for (char d = '0'; d <= '9'; d++) {
        uart_putchar(d);
    }
    uart_puts("\n[uart-test] done.\n");

    for (;;) {
        /* 停机：真实固件里这里通常用 wfi 省电 */
    }
}
