#include <sbi_utils/i2c/fdt_i2c.h>

extern struct fdt_i2c_adapter fdt_i2c_adapter_sifive;

struct fdt_i2c_adapter *fdt_i2c_adapter_drivers[] = {
	&fdt_i2c_adapter_sifive,
};

unsigned long fdt_i2c_adapter_drivers_size = sizeof(fdt_i2c_adapter_drivers) / sizeof(struct fdt_i2c_adapter *);
