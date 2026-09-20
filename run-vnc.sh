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
#   Ctrl-Alt-1 -> 普通世界 (OpenSBI + timeros, UART0)   ← 主要输出
#   Ctrl-Alt-2 -> UART1 (备用)
#   Ctrl-Alt-3 -> 可信域 FreeRTOS (UART2)
#   Ctrl-Alt-4 -> QEMU monitor
#   (若 VNC 客户端拦截了 Ctrl-Alt, 用其"发送按键"功能, 或改用无图形脚本)
# =============================================================================
SHELL_FOLDER=$(cd "$(dirname "$0")";pwd)
DEFAULT_VC="960x960"

# VNC 监听地址:
#   默认 127.0.0.1:0  = 端口 5900, 仅本机可达, 需配合上面的 SSH LocalForward(安全, 推荐)
#   若想在局域网直连(需开放防火墙 5900, 注意安全), 运行时传参:  ./run-vnc.sh :0
VNC_DISPLAY="${1:-127.0.0.1:0}"

echo "[run-vnc] 启动 quard-star, VNC 显示 = ${VNC_DISPLAY} (display :0 = 端口 5900)"
echo "[run-vnc] 客户端 VNC Viewer 连接 localhost:5900 (经 SSH LocalForward)"
echo "[run-vnc] 控制台切换: Ctrl-Alt-1 普通世界 / -3 可信域 / -4 monitor"
echo "[run-vnc] 关闭: 在 monitor 控制台输入 quit, 或 Ctrl-C 结束本脚本"

$SHELL_FOLDER/output/qemu/bin/qemu-system-riscv64 \
-M quard-star \
-m 1G \
-smp 8 \
-bios none \
-drive if=pflash,bus=0,unit=0,format=raw,file=$SHELL_FOLDER/output/fw/fw.bin \
-vnc $VNC_DISPLAY \
-serial vc:$DEFAULT_VC \
-serial vc:$DEFAULT_VC \
-serial vc:$DEFAULT_VC \
-monitor vc:$DEFAULT_VC \
--parallel none

# 如需指令级反汇编日志(会生成很大的 qemu.log, 调试用), 把下面这行加到上面参数里:
#   -d in_asm -D qemu.log
