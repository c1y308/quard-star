#include <sbi_utils/gpio/fdt_gpio.h>

extern struct fdt_gpio fdt_gpio_sifive;

struct fdt_gpio *fdt_gpio_drivers[] = {
	&fdt_gpio_sifive,
};

unsigned long fdt_gpio_drivers_size = sizeof(fdt_gpio_drivers) / sizeof(struct fdt_gpio *);
