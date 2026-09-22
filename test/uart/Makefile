# =============================================================================
# quard-star UART 裸机测试固件构建脚本
#
# 仅编译 start.S + uart_test.c 组成的最小验证固件, 产物统一输出到 build/,
# 与正式启动链(build.sh)完全解耦, 不修改任何源文件。
#
# 用法:
#   make            # 编译并打包 32MB pflash 镜像 build/test_fw.bin
#   make run        # 用项目自带 QEMU 运行, 串口输出重定向到终端
#   make clean      # 清空 build/
# =============================================================================

CROSS_COMPILE = riscv64-unknown-elf-
CC      = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

# 关键编译参数(缺一不可):
#   rv64imac_zicsr : 新版 binutils 下 csrr 指令需要显式 zicsr 扩展, 否则汇编失败
#   medany         : 访问 0x80100000 栈地址(>2GB)必须, 默认 medlow 会溢出
#   freestanding   : 裸机环境, 无标准库/无操作系统
ARCH_FLAGS = -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany
CFLAGS     = $(ARCH_FLAGS) -nostdlib -fno-builtin -ffreestanding -g -Wall

BUILD  = build
TARGET = uart_test

# 链接脚本与 Map/反汇编产物
LDFLAGS = -nostartfiles -T uart.lds -Wl,-Map=$(BUILD)/$(TARGET).map

# pflash 镜像: 32MB 全零填充, 固件 dd 到偏移 0(对应 CPU 物理地址 0x20000000)
FLASH_IMG    = $(BUILD)/test_fw.bin
FLASH_SIZE_MB = 32

# 项目自带 QEMU(run.sh 使用的同一份), 可用 make QEMU=... 覆盖
QEMU ?= ../../output/qemu/bin/qemu-system-riscv64

OBJS = $(BUILD)/start.o $(BUILD)/uart_test.o

.PHONY: all clean run

all: $(FLASH_IMG)

# 产物目录
$(BUILD):
	mkdir -p $(BUILD)

# 打包成 32MB pflash 镜像
$(FLASH_IMG): $(BUILD)/$(TARGET).bin  # 依赖 uart_test.bin
	dd if=/dev/zero of=$@ bs=1M count=$(FLASH_SIZE_MB) 2>/dev/null
	dd if=$< of=$@ bs=1k conv=notrunc seek=0
	@echo ">>> 已生成 $$(du -h $@ | cut -f1) 的 pflash 镜像: $@"

# 生成 uart_test.bin
$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf # 依赖 uart_test.elf
	$(OBJCOPY) -O binary -S $< $@

# 生成 uart_test.elf, 并生成反汇编清单便于调试
$(BUILD)/$(TARGET).elf: $(OBJS) uart.lds  # 依赖 start.o + uart_test.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)
	$(OBJDUMP) --source --demangle --disassemble --reloc --wide $@ > $(BUILD)/$(TARGET).lst

# 汇编文件 -> .o目标文件
$(BUILD)/start.o: start.S | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

# C文件 -> .o目标文件
$(BUILD)/uart_test.o: uart_test.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<


clean:
	rm -rf $(BUILD)

# 一键运行: -M quard-star, 单核, 串口0 接到终端, 其余串口置空
run: $(FLASH_IMG)
	$(QEMU) -M quard-star -smp 1 -m 1G -bios none \
		-drive if=pflash,bus=0,unit=0,format=raw,file=$(FLASH_IMG) \
		-display none -serial mon:stdio -serial null -serial null
