OUTPUT_ARCH("riscv")
ENTRY(_mstart)

_STACK_SIZE = DEFINED(_STACK_SIZE) ? _STACK_SIZE : 0x1000;

SECTIONS
{
	. = 0x10000000;
	.text : {	*(.text)	}

	.data : {
	_gp = . + 0x800;
		*(.data)
	}

    . = ALIGN(0x1000);
	.stack : {
       _stack_end = .;
       . += _STACK_SIZE;
       . = ALIGN(4);
       _stack = .;
       __stack = _stack;
    }
    
    .bss : {
    	. = ALIGN(4);
    	__bss_start = .;
       *(.bss)
       . = ALIGN(4);
       __bss_end = .; 
    }

}
