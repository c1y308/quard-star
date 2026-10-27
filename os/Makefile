
CROSS_COMPILE = riscv64-unknown-elf-
CFLAGS = -nostdlib -fno-builtin -mcmodel=medany -g

GDB = riscv64-unknown-elf-gdb
# riscv64-unknown-elf-gcc 工具链可以同时编译汇编和 C 代码
CC = ${CROSS_COMPILE}gcc
OBJCOPY = ${CROSS_COMPILE}objcopy
OBJDUMP = ${CROSS_COMPILE}objdump

SRCS_ASM = \
	entry.S \
	kerneltrap.S \
	kernel_irq.S \
	switch.S

SRCS_C = \
	sbi.c \
	main.c \
	printk.c \
	trap.c \
	syscall.c \
	string.c \
	app.c \
	timer.c \
	stack.c \
	address.c \
	task.c \
	assert.c \
	vsprintf.c \
	printf.c \
	loader.c \
	virtio_disk.c \
	irq.c \
	uart.c \
	bio.c \
	easyfs.c \
	fs.c

INCLUDE:=-I./include
SRC:=./src
LIB:=./lib

# 将源文件替换为 .o 文件结尾的文件名
OBJS = $(SRCS_ASM:.S=.o)
OBJS += $(SRCS_C:.c=.o)


all: fs-image os.elf

os.elf: ${OBJS}
	${CC} ${CFLAGS} -T os.ld -Wl,-Map=os.map -o os.elf $^
	${OBJCOPY} -O binary os.elf os.bin

%.o : $(SRC)/%.c
	${CC} ${CFLAGS} $(INCLUDE) -c -o $@ $<

%.o : $(LIB)/%.c
	${CC} ${CFLAGS} $(INCLUDE) -c -o $@ $<

%.o : $(SRC)/%.S
	${CC} ${CFLAGS} $(INCLUDE) -c -o $@ $<

build_app:
	$(MAKE) -B -C ./user

mkfs.out: tools/mkfs.c src/easyfs.c include/timeros/easyfs.h
	gcc -std=c11 -Wall -Wextra -I./include tools/mkfs.c src/easyfs.c -o $@

fs-image: build_app mkfs.out
	mkdir -p ../output/disk
	./mkfs.out ../output/disk/fs.img ../output/user/bin

fs-reset: build_app mkfs.out
	mkdir -p ../output/disk
	./mkfs.out ../output/disk/fs.img ../output/user/bin --reset

test_easyfs.out: tools/test_easyfs.c src/easyfs.c include/timeros/easyfs.h
	gcc -std=c11 -Wall -Wextra -I./include tools/test_easyfs.c src/easyfs.c -o $@

fs-test: test_easyfs.out
	./test_easyfs.out

.PHONY: build_app
.PHONY: compile-commands
compile-commands:
	python3 tools/generate_compile_commands.py

.PHONY: fs-image fs-reset fs-test
.PHONY : debug
debug: 
	@echo "os debug start..."
	@${GDB} os.elf -q -x ./gdbinit

.PHONY : clean
clean:
	rm -f *.o *.bin *.out os.elf os.map
