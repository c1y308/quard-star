# quard-star
从零基于qemu创建riscv嵌入式开发板，并移植操作系统。
文档教程地址：[分类: QEMU中自定义开发板 | TimerのBlog (yanglianoo.github.io)](https://yanglianoo.github.io/categories/%E5%9F%BA%E4%BA%8Eqemu%E4%BB%8E%E9%9B%B6%E5%BC%80%E5%A7%8B%E6%9E%84%E5%BB%BAriscv64%E7%9A%84%E5%B5%8C%E5%85%A5%E5%BC%8F%E7%B3%BB%E7%BB%9F/)

## easy-fs 文件系统

`./build.sh` 构建内核和用户程序，并创建或更新 `output/disk/fs.img`。镜像与 rCore Tutorial v3 的 easy-fs 格式兼容，运行脚本会将其作为 virtio 磁盘接入。后续构建只更新与用户程序同名的文件，其他文件会保留；这些程序名应视为保留文件名。原有 `output/disk/disk.img` 不会被覆盖。

在 `os` 目录运行 `make fs-test` 可执行文件系统核心测试。仅在希望清空镜像全部内容时运行 `make fs-reset`。启动后可在 shell 输入 `filetest_simple`、`filetest_persist` 和 `filetest_fd` 验证文件读写、重启持久性及文件描述符行为。

## 设备中断

内核在建立页表时映射 PLIC、UART0 和 virtio MMIO；安装内核 trap 入口、设置下一次时钟中断后，`device_init()` 配置 hart 0 的 PLIC Supervisor 上下文并开启外部中断。quard-star 的 virtio 块设备使用 IRQ 1，UART0 使用 IRQ 10；PLIC 遵循标准优先级映射，ID 0 保留、ID n 位于 `PLIC_BASE + 4*n`。用户态和内核态的外部中断统一经过 PLIC claim、设备处理与 complete。磁盘请求由 IRQ 确认完成，UART0 接收中断将输入放入内核缓冲区；控制台输出仍经 SBI。

## clangd 函数跳转

在仓库目录运行 `make -C os compile-commands`，会根据内核、用户程序和宿主机文件系统工具的 Makefile 生成 `os/compile_commands.json`；`./build.sh` 也会更新它。clangd 从该文件读取各源文件的实际编译参数。修改 Makefile 中的源文件或编译选项后，请重新生成，再在编辑器中重启 clangd。

QEMU 源码由 `qemu-11.1.1/.clangd` 指向同级工作区的 `qemu-build/compile_commands.json`。首次使用前需按 `build.sh` 的方式配置 QEMU，使 Meson 生成该文件。建议在编辑器中打开整个仓库，让 clangd 的后台索引完成后再使用跨文件“转到定义”。
