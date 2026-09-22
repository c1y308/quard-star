OUTPUT_ARCH( "riscv" )

ENTRY( _start )

MEMORY
{
    /* 与 boot/boot.lds 一致：固件在 Flash 基址 0x20000000 就地执行(XIP) */
    flash (rxai!w) : ORIGIN = 0x20000000, LENGTH = 512k
}

SECTIONS
{
    .text : {
        KEEP(*(.text.init))   /* 复位入口 _start 必须排在最前 */
        *(.text*)
    } >flash

    .rodata : { *(.rodata*) } >flash
}
