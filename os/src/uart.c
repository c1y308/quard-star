#include <timeros/os.h>

#define UART_RBR 0
#define UART_IER 1
#define UART_LCR 3
#define UART_MCR 4
#define UART_LSR 5
#define UART_LSR_RX_READY 0x01
#define UART_RX_CAPACITY 256

static unsigned char rx_buffer[UART_RX_CAPACITY];
static unsigned int rx_read;
static unsigned int rx_write;
static unsigned int rx_count;

static volatile unsigned char *uart_reg(unsigned int offset)
{
    return (volatile unsigned char *)(UART0_BASE + offset);
}

void uart0_init(void)
{
    /* OpenSBI configured the baud rate. Keep it and enable RX interrupts. */
    *uart_reg(UART_LCR) &= ~0x80U;
    *uart_reg(UART_MCR) = 0x0b; /* DTR, RTS, OUT2 */
    *uart_reg(UART_IER) = 0x01;
}


int uart0_getchar(void)
{
    reg_t old_status = r_sstatus();
    intr_off();
    while (rx_count == 0) {
        /* Yield to another runnable thread, then sleep if still empty. */
        schedule();
        if (rx_count == 0) {
            intr_on();
            asm volatile("wfi" ::: "memory");
            intr_off();
        }
    }
    int ch = rx_buffer[rx_read];
    rx_read = (rx_read + 1) % UART_RX_CAPACITY;
    --rx_count;
    w_sstatus(old_status);
    return ch;
}


void uart0_irq(void)
{
    while (*uart_reg(UART_LSR) & UART_LSR_RX_READY) {
        unsigned char ch = *uart_reg(UART_RBR);
        if (rx_count < UART_RX_CAPACITY) {
            rx_buffer[rx_write] = ch;
            rx_write = (rx_write + 1) % UART_RX_CAPACITY;
            ++rx_count;
        }
        /* When full, discard the newest byte but still drain the UART. */
    }
}

