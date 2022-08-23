ORG	0x90000
BITS	32

	jmp	start

kentry	dd	0		; kernel entry point - set by bsd-ce

start:	; kill irq's and nmi's as quickly as possible - this is important,
	; as we'd crash later on without this
	cli
	mov	al,0x80
	out	0x70,al

	; the following serial port init code is an assembler version of
	; http://www.osdev.org/wiki/Serial_ports

	; disable serial interrupts
	mov	dx,0x3f8+1
	mov	al,0x00
	out	dx,al
	; enable DLAB (set baud rate divisor)
	mov	dx,0x3f8+3
	mov	al,0x80
	out	dx,al
	; set divisor lo byte to 0xc (9600 baud)
	mov	dx,0x3f8+0
	mov	al,0x0c
	out	dx,al
	; set divisor hi byte to 0
	mov	dx,0x3f8+1
	mov	al,0x0
	out	dx,al
	; 8 bits, no parity, one stop bit
	mov	dx,0x3f8+3
	mov	al,0x3
	out	dx,al
	; enable fifo, clear it and with 14-byte threshold
	mov	dx,0x3f8+2
	mov	al,0xc7
	out	dx,al
	; enable irqs
	mov	dx,0x3f8+4
	mov	al,0x0b
	out	dx,al

	; hello world
	mov	al,'B'
	call	putch
	mov	al,'S'
	call	putch
	mov	al,'D'
	call	putch
	mov	al,'!'
	call	putch
	mov	al,0xa
	call	putch
	mov	al,0xd
	call	putch

	; set up the freebsd stack values
	push	bootinfo	; bootinfo
	push	0x0		; 0
	push	0x0		; 0
	push	0x0		; 0
	push	0x0		; bootdev
	push	0x80001000	; howto (bootinfo, serial console)
				; look at sys/reboot.h for more
	push	retaddr		; return address
	mov	ebx,[kentry]
	jmp	ebx

retaddr:
	; if we got here, something went wrong - so keep printing
	; question marks as a desperate cry for help
	push	retaddr
	mov	al,'?'

; outputs al on com1
putch:
	push	eax

	; poll the LSR to ensure it's not sending
	mov	dx,0x3f8+5
poll_lsr:
	in	al,dx
	test	al,0x20		; Transmitter Holding Register Empty?
	jz	poll_lsr	; no - try again

	pop	eax

	; put the char in the data register
	mov	dx,0x3f8
	out	dx,al

	; wait until it goes...
	mov	dx,0x3f8+5
poll_lsr2:
	in	al,dx
	test	al,0x20		; Transmitter Holding Register Empty?
	jz	poll_lsr2	; no - try again
	ret

bootinfo:
	dd	1		; bi_version
	dd	0		; bi_kernelname (pointer)
	dd	0		; bi_nfs_diskless
	dd	0		; bi_n_bios_used
	times 8 dd 0		; bi_bios_geom[N_BIOS_GEOM=8]
	dd	0x54		; bi_size
	db	0		; bi_memsizes_valid
	db	0		; bi_bios_dev
	times 2 db 0		; bi_pad
	dd	0		; bi_basemem
	dd	0		; bi_extmem
	dd	0		; bi_symtab
	dd	0		; bi_esymtab
	dd	0		; bi_kernend
	dd	envp		; bi_envp	
	dd	0		; bi_modulep

envp	db	'machdep.bios.pci=disable',0
	db	'machdep.bios.pnp=disable',0
	db	'vfs.root.mountfrom=ufs:/dev/da0a',0
	db	0
