OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
 . = 0xBFF80000;

 PROVIDE(_fw_start = .);

 . = ALIGN(0x1000);



 .text :
  {
  PROVIDE(_text_start = .);
  *(.entry)
  *(.text)
  . = ALIGN(8);
  PROVIDE(_text_end = .);
 }



 . = ALIGN(0x1000);



 .rodata :
 {
  PROVIDE(_rodata_start = .);
  *(.rodata .rodata.*)
  . = ALIGN(8);
  PROVIDE(_rodata_end = .);
 }



 . = ALIGN(0x1000);



 .data :
 {
  PROVIDE(_data_start = .);

  *(.sdata)
  *(.sdata.*)
  *(.data)
  *(.data.*)
  *(.readmostly.data)
  *(*.data)
  . = ALIGN(8);

  PROVIDE(_data_end = .);
 }

 .dynsym : {
  PROVIDE(__dyn_sym_start = .);
  *(.dynsym)
  PROVIDE(__dyn_sym_end = .);
 }

 .rela.dyn : {
  PROVIDE(__rel_dyn_start = .);
  *(.rela*)
  . = ALIGN(8);
  PROVIDE(__rel_dyn_end = .);
 }

 . = ALIGN(0x1000);

 .bss :
 {
  PROVIDE(_bss_start = .);
  *(.sbss)
  *(.sbss.*)
  *(.bss)
  *(.bss.*)
  . = ALIGN(8);
  PROVIDE(_bss_end = .);
 }



 . = ALIGN(0x1000);

 PROVIDE(_fw_end = .);

 PROVIDE(_fw_reloc_end = .);
}
