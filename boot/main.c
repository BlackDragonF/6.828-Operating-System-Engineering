#include <inc/x86.h>
#include <inc/elf.h>

/**********************************************************************
 * This a dirt simple boot loader, whose sole job is to boot
 * an ELF kernel image from the first IDE hard disk.
 *
 * DISK LAYOUT
 *  * This program(boot.S and main.c) is the bootloader.  It should
 *    be stored in the first sector of the disk.
 *
 *  * The 2nd sector onward holds the kernel image.
 *
 *  * The kernel image must be in ELF format.
 *
 * BOOT UP STEPS
 *  * when the CPU boots it loads the BIOS into memory and executes it
 *
 *  * the BIOS intializes devices, sets of the interrupt routines, and
 *    reads the first sector of the boot device(e.g., hard-drive)
 *    into memory and jumps to it.
 *
 *  * Assuming this boot loader is stored in the first sector of the
 *    hard-drive, this code takes over...
 *
 *  * control starts in boot.S -- which sets up protected mode,
 *    and a stack so C code then run, then calls bootmain()
 *
 *  * bootmain() in this file takes over, reads in the kernel and jumps to it.
 **********************************************************************/

#define SECTSIZE	512
#define ELFHDR		((struct Elf *) 0x10000) // scratch space

void readsect(void*, uint32_t);
void readseg(uint32_t, uint32_t, uint32_t);

void
bootmain(void)
{
	struct Proghdr *ph, *eph;

	// read 1st page off disk
	readseg((uint32_t) ELFHDR, SECTSIZE*8, 0);

	// is this a valid ELF?
	if (ELFHDR->e_magic != ELF_MAGIC)
		goto bad;

	// load each program segment (ignores ph flags)
	ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
	eph = ph + ELFHDR->e_phnum;
	for (; ph < eph; ph++)
		// p_pa is the load address of this segment (as well
		// as the physical address)
		readseg(ph->p_pa, ph->p_memsz, ph->p_offset);

	// call the entry point from the ELF header
	// note: does not return!
	((void (*)(void)) (ELFHDR->e_entry))();

bad:
	// stops simulation and breaks into the debug console
	outw(0x8A00, 0x8A00);
	outw(0x8A00, 0x8E00);
	while (1)
		/* do nothing */;
}

// Read 'count' bytes at 'offset' from kernel into physical address 'pa'.
// Might copy more than asked
void
readseg(uint32_t pa, uint32_t count, uint32_t offset)
{
	uint32_t end_pa;

	end_pa = pa + count;

	// round down to sector boundary
	pa &= ~(SECTSIZE - 1);

	// translate from bytes to sectors, and kernel starts at sector 1
	offset = (offset / SECTSIZE) + 1;

	// If this is too slow, we could read lots of sectors at a time.
	// We'd write more to memory than asked, but it doesn't matter --
	// we load in increasing order.
	while (pa < end_pa) {
		// Since we haven't enabled paging yet and we're using
		// an identity segment mapping (see boot.S), we can
		// use physical addresses directly.  This won't be the
		// case once JOS enables the MMU.
		readsect((uint8_t*) pa, offset);
		pa += SECTSIZE;
		offset++;
	}
}

void
waitdisk(void)
{
	// wait for disk reaady
	while ((inb(0x1F7) & 0xC0) != 0x40)
		/* do nothing */;
	// 0x1F7 Disk 0 status
	// Status register:
	/* bit 6    : RDY bit. indicates that the disk has finished its
             power-up. Wait for this bit to be active before doing
             anything (execpt reset) with the disk. I once ignored
             this bit and was rewarded with a completely unusable
             disk.
	  bit 7    : BSY bit. This bit is set when the disk is doing
             something for you. You have to wait for this bit to
             clear before you can start giving orders to the disk. */
	// MASK: 11000000
}

void
readsect(void *dst, uint32_t offset)
{
	// wait for disk to be ready
	waitdisk();

	outb(0x1F2, 1);		// count = 1 0x1F2 Disk 0 sector count
	// Read one sector each time
	outb(0x1F3, offset); // Disk 0 sector number
	// First sector's number
	outb(0x1F4, offset >> 8); // Cylinder low
	outb(0x1F5, offset >> 16); // Cylinder high
	// Cylinder number
	outb(0x1F6, (offset >> 24) | 0xE0); // Disk 0 drive/head
	// MASK 11100000
	// Drive/Head Register: bit 7 and bit 5 should be set to 1
	// Bit6: 1 LBA mode, 0 CHS mode
	outb(0x1F7, 0x20);	// cmd 0x20 - read sectors
	/*20H       Read sector with retry. NB: 21H = read sector
                without retry. For this command you have to load
                the complete circus of cylinder/head/sector
                first. When the command completes (DRQ goes
                active) you can read 256 words (16-bits) from the
                disk's data register. */


	// wait for disk to be ready
	waitdisk();

	// read a sector
	insl(0x1F0, dst, SECTSIZE/4);
	// Data register: data exchange with 8/16 bits
	// insl port addr cnt: read cnt dwords from the input port
	// specified by port into the supplied output array addr.
	// dword: 4 bytes
}
