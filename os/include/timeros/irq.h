#ifndef TOS_IRQ_H__
#define TOS_IRQ_H__

#include <timeros/types.h>

#define PLIC_BASE 0x0c000000UL
#define PLIC_SIZE 0x210000UL
#define UART0_BASE 0x10000000UL
#define VIRTIO0_IRQ 1U
#define UART0_IRQ 10U

void device_init(void);
void irq_handler(void);
void uart0_init(void);
void uart0_irq(void);
int uart0_getchar(void);

#endif
