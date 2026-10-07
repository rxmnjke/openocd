OUTPUT_FORMAT("elf32-littleriscv", "elf32-littleriscv", "elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_start)
MEMORY {
  ram (RWX): ORIGIN = 0x02000000, LENGTH = 16K
}

SECTIONS {
    .text ORIGIN(ram) : {
        *(.start)
        *(.text)
    } >ram
    /DISCARD/ : {
        *(.eh_frame .eh_frame.*)
    }
}
