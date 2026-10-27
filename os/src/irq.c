#include <timeros/os.h>

/* QEMU quard-star uses context 0 for hart 0 M-mode and context 1 for S-mode. */
#define PLIC_S_CONTEXT 1UL
/* Standard PLIC map: source 0 is reserved, source n is at 4 * n. */
#define PLIC_PRIORITY(id) (PLIC_BASE + (id) * 4UL)
#define PLIC_ENABLE (PLIC_BASE + 0x2000UL + PLIC_S_CONTEXT * 0x80UL)
#define PLIC_THRESHOLD (PLIC_BASE + 0x200000UL + PLIC_S_CONTEXT * 0x1000UL)
#define PLIC_CLAIM (PLIC_THRESHOLD + 4UL)

static volatile u32 *plic_reg(reg_t address)
{
    return (volatile u32 *)address;
}

void device_init(void)
{
    uart0_init();

    /* A priority of 1 passes threshold 0 on the supervisor context. */
    *plic_reg(PLIC_THRESHOLD) = 0;
    *plic_reg(PLIC_PRIORITY(VIRTIO0_IRQ)) = 1;
    *plic_reg(PLIC_PRIORITY(UART0_IRQ)) = 1;
    *plic_reg(PLIC_ENABLE) = (1U << VIRTIO0_IRQ) | (1U << UART0_IRQ);
    w_sie(r_sie() | SIE_SEIE);
}

void irq_handler(void)
{
    u32 id;
    while ((id = *plic_reg(PLIC_CLAIM)) != 0) {
        switch (id) {
        case VIRTIO0_IRQ:
            virtio_disk_intr();
            break;
        case UART0_IRQ:
            uart0_irq();
            break;
        default:
            *plic_reg(PLIC_CLAIM) = id;
            printk("unexpected PLIC IRQ %d\n", id);
            panic("unexpected PLIC IRQ");
        }
        *plic_reg(PLIC_CLAIM) = id;
    }
}
