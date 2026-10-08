#!/bin/bash
# =============================================================================
# quard-star VNC 运行脚本
# 适用场景: 通过 SSH / 无本地图形显示(DISPLAY 为空)运行时, 用 VNC 代替 GTK 的
#           vc 窗口。服务端 headless 运行, 不需要 X11 转发。
#
# 客户端(Windows)使用步骤:
#   1. 安装 VNC Viewer (RealVNC / TightVNC / TigerVNC 均可)
#      注意: VcXsrv 是 X 服务器, 不能当 VNC 客户端用!
#   2. 在 ~/.ssh/config 的 "Host 192.168.5.57" 段里加一行端口转发:
#        LocalForward 5900 127.0.0.1:5900
#   3. 重新连接 SSH(让转发生效), 然后在服务端运行本脚本:
#        cd /home/c1y308/quard-star/quard-star && ./run-vnc.sh
#   4. 客户端打开 VNC Viewer, 连接地址: localhost:5900
#
# VNC 窗口内切换虚拟控制台 (按住 Ctrl-Alt 再按数字):
#   monitor 已移到终端 stdio, 不再抢占 VNC 控制台; 3 个串口 vc 即 console0~2,
#   VNC 默认视图 = console0 = UART0, 连上即可看主输出, 无需切换
#   Ctrl-Alt-1 -> UART0 (普通世界 OpenSBI+timeros / 测试固件)   ← 主要输出 & 默认
#   Ctrl-Alt-2 -> UART1 (备用)
#   Ctrl-Alt-3 -> UART2 (可信域当前固件的输出)
#   (运行本脚本的终端即 QEMU monitor, 输入 quit 退出)
#   (若 VNC 客户端拦截了 Ctrl-Alt, 用其"发送按键"功能, 或改用无图形脚本)
# =============================================================================
SHELL_FOLDER=$(cd "$(dirname "$0")";pwd)
DEFAULT_VC="960x960"

# VNC 监听地址:
#   默认 127.0.0.1:0  = 端口 5900, 仅本机可达, 需配合上面的 SSH LocalForward(安全, 推荐)
#   若想在局域网直连(需开放防火墙 5900, 注意安全), 运行时传参:  ./run-vnc.sh :0
VNC_DISPLAY="${1:-127.0.0.1:0}"

# 要运行的固件镜像(二选一: 注释掉当前行, 取消注释另一行即可切换):
#   默认 = 正式启动链 fw.bin (lowlevel_fw -> openSBI -> timeros/可信域)
FW_IMAGE="$SHELL_FOLDER/output/fw/fw.bin"
#   可选 = UART 裸机测试固件(需先在 test/uart 下执行 make), 输出见 VNC 控制台 Ctrl-Alt-1
# FW_IMAGE="$SHELL_FOLDER/test/uart/build/test_fw.bin"

echo "[run-vnc] 启动 quard-star, VNC 显示 = ${VNC_DISPLAY} (默认 :0 对应 TCP 5900)"
echo "[run-vnc] 运行固件 = ${FW_IMAGE}"
echo "[run-vnc] 默认配置下，VNC Viewer 连接 localhost:5900 (经 SSH LocalForward)"
echo "[run-vnc] VNC 画面切换: Ctrl-Alt-1 UART0 / Ctrl-Alt-2 UART1 / Ctrl-Alt-3 UART2 (同一 VNC 端口)"
echo "[run-vnc] QEMU monitor 在运行脚本的终端；输入 quit 或按 Ctrl-C 结束"

$SHELL_FOLDER/output/qemu/bin/qemu-system-riscv64 \
-M quard-star \
-m 1G \
-smp 8 \
-bios none \
-drive if=pflash,bus=0,unit=0,format=raw,file=$FW_IMAGE \
-drive file=$SHELL_FOLDER/output/disk/disk.img,if=none,format=raw,id=x0 \
-device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
-vnc $VNC_DISPLAY \
-serial vc:$DEFAULT_VC \
-serial vc:$DEFAULT_VC \
-serial vc:$DEFAULT_VC \
-monitor stdio \
--parallel none

# 如需指令级反汇编日志(会生成很大的 qemu.log, 调试用), 把下面这行加到上面参数里:
#   -d in_asm -D qemu.log
