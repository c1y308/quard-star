CROSS_COMPILE = riscv64-unknown-elf-
CFLAGS = -nostdlib -fno-builtin -mcmodel=medany

CC = ${CROSS_COMPILE}gcc
OBJCOPY = ${CROSS_COMPILE}objcopy
OBJDUMP = ${CROSS_COMPILE}objdump
INCLUDE:=-I../include

LIB = ../lib
BIN_DIR = ../../output/user/bin

all: $(BIN_DIR) initproc xec user_shell time thread_test thread_main_exit thread_capacity_test thread_process_exit filetest_simple filetest_persist filetest_fd

filetest_simple: filetest_simple.c $(LIB)/*.c
	${CC} ${CFLAGS} $(INCLUDE) -T user.ld $^ -o $(BIN_DIR)/filetest_simple

filetest_persist: filetest_persist.c $(LIB)/*.c
	${CC} ${CFLAGS} $(INCLUDE) -T user.ld $^ -o $(BIN_DIR)/filetest_persist

filetest_fd: filetest_fd.c $(LIB)/*.c
	${CC} ${CFLAGS} $(INCLUDE) -T user.ld $^ -o $(BIN_DIR)/filetest_fd

thread_process_exit: thread_process_exit.c $(LIB)/*.c
	${CC} ${CFLAGS} $(INCLUDE) -T user.ld $^ -o $(BIN_DIR)/thread_process_exit

thread_capacity_test: thread_capacity_test.c $(LIB)/*.c
	${CC} ${CFLAGS} $(INCLUDE) -T user.ld $^ -o $(BIN_DIR)/thread_capacity_test

thread_test: thread_test.c $(LIB)/*.c
	${CC} ${CFLAGS} $(INCLUDE) -T user.ld $^ -o $(BIN_DIR)/thread_test

thread_main_exit: thread_main_exit.c $(LIB)/*.c
	${CC} ${CFLAGS} $(INCLUDE) -T user.ld $^ -o $(BIN_DIR)/thread_main_exit

# 创建bin目录的规则
$(BIN_DIR):
	mkdir -p $@

write: write.c $(LIB)/*.c
	${CC} ${CFLAGS} $(INCLUDE) -T user.ld $^ -o $(BIN_DIR)/write

time: time.c $(LIB)/*.c
	${CC} ${CFLAGS} $(INCLUDE) -T user.ld $^ -o $(BIN_DIR)/time

user_shell:user_shell.c $(LIB)/*.c
	${CC} ${CFLAGS} $(INCLUDE) -T user.ld  $^ -o $(BIN_DIR)/user_shell

xec: xec.c $(LIB)/*.c
	${CC} ${CFLAGS} $(INCLUDE) -T user.ld  $^ -o $(BIN_DIR)/xec

initproc: initproc.c $(LIB)/*.c
	${CC} ${CFLAGS} $(INCLUDE) -T user.ld $^ -o $(BIN_DIR)/initproc

debug: objdump_time objdump_write objdump_xec

objdump_time:
	${OBJDUMP} -d $(BIN_DIR)/time > time.txt
objdump_write:
	${OBJDUMP} -d $(BIN_DIR)/write > write.txt
objdump_xec:
	${OBJDUMP} -d $(BIN_DIR)/xec > xec.txt

.PHONY: all clean
clean:
	rm -f $(BIN_DIR)/*
