#include <sys/types.h>
#include <fcntl.h>
#include <gelf.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define ADDRESS_MASK 0x00ffffff

char*	outfile    = "nk.bin";
char* loaderfile = "loader";
int	verbose = 0;
FILE*	fout = NULL;
FILE*	kfile = NULL;
FILE* lfile = NULL;
char* loader = NULL;
uint32_t loader_size = 0;
uint32_t loader_entry_offs = 5;

#define WRITE(arg,size) \
	if(!fwrite(arg, size, 1, fout)) \
		errx(1,"fwrite");

uint8_t  magic[7] = "B000FF\n";
uint32_t start = 0x90000;
uint32_t len   = 0;

void
usage()
{
	fprintf(stderr, "usage: bsd-ce [-hv] [-l loader] [-e offs] [-o outfile] kernel\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -h                 this help\n");
	fprintf(stderr, "    -v                 increase verbosity\n");
	fprintf(stderr, "    -l loader          file to use as loader\n");
	fprintf(stderr, "                       default: %s\n", loaderfile);
	fprintf(stderr, "    -e offs            offset to patch entrypoint in loader\n");
	fprintf(stderr, "                       default: 0x%x\n", loader_entry_offs);
	fprintf(stderr, "    -o outfile         write output to [outfile]\n");
	fprintf(stderr, "                       default: %s\n", outfile);
}

int
main(int argc, char* argv[])
{
	int n, i;
	uint32_t i32, cksum, ch;
	long len;
	Elf* e;
	GElf_Ehdr ehdr;
	GElf_Phdr phdr;

	while((i = getopt(argc, argv, "hvo:l:")) != -1) {
		switch(i) {
			case 'v':
				verbose++;
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'l':
				loaderfile  = optarg;
				break;
			case 'h':
			default:
				usage();
				return 1;
		}
	}
	argc -= optind; argv += optind;
	if (argc != 1) {
		fprintf(stderr, "missing kernel name\n");
		usage();
		return 1;
	}

	if (elf_version(EV_CURRENT) == EV_NONE)
		errx(1, "libelf initialization failed");

	/* open the kernel and parse the ELF sections*/
	if ((kfile = fopen(argv[0], "rb")) == NULL)
		errx(1, "unable to open '%s'", argv[0]);
	if ((e = elf_begin(fileno(kfile), ELF_C_READ, NULL)) == NULL)
		errx(1, "elf_begin() failed: %s", elf_errmsg(-1));
	if (elf_kind(e) != ELF_K_ELF)
		err(1, "supplied kernel is not an ELF object");
	if (gelf_getehdr(e, &ehdr) == NULL)
		errx(1, "gelf_gethedr() failed: %s", elf_errmsg(-1));
	if (gelf_getclass(e) != ELFCLASS32)
		err(1, "supplied kernel is not a 32-bit ELF file");

	/* load the loader (err...) */
	if ((lfile = fopen(loaderfile, "rb")) == NULL)
		errx(1, "unable to open '%s'", loaderfile);
	fseek(lfile, 0, SEEK_END); loader_size = ftell(lfile); fseek(lfile, 0, SEEK_SET);
	if ((loader = (char*)malloc(loader_size)) == NULL)
		errx(1, "can't allocate %u bytes for loader", loader_size);
	if (!fread(loader, loader_size, 1, lfile))
		errx(1, "can't read loader file");
	fclose(lfile);
	/* NB: This is not completely true, but saves us having to pad the loader */
	if (loader_entry_offs >= loader_size)
		err(1, "loader entry offset cannot be beyond loader length\n");
	
	// next, try to create the output file
	if ((fout = fopen(outfile, "wb")) == NULL)
		errx(1, "unable to create '%s'", outfile);

	/*
	 * We must calculate the total length in use beforehand.
	 */
	if (elf_getphnum(e, &n) == 0)
		errx(1, "elf_getphnum() failed: %s", elf_errmsg(-1));
	len = 0;
	for (i = 0; i < n; i++) {
		if (gelf_getphdr(e, i, &phdr) != &phdr)
			errx(1, "elf_getphdr() failed: %s", elf_errmsg(-1));

		/* Skip anything not loadable */
		if (phdr.p_type != PT_LOAD)
			continue;

		i32 = (uint32_t)(phdr.p_paddr + phdr.p_memsz) & ADDRESS_MASK;
		if (len < i32)
			len = i32;
	}
	if (verbose)
		printf("Final image offset is %x\n", len);

	/*
   * Write NK.BIN header - this is a magic identifier, the first address
	 * data is stored, and the total length
	 */
	WRITE(magic,  sizeof(magic));
	WRITE(&start, sizeof(start));
	i32 = len - start; WRITE(&i32,  sizeof(i32));

	/*
	 * The first step is to get the loader in there - BSD kernels do not
	 * appreciate just being jumped to, they want some stuff on the stack
	 * as well; that's what the loader does for us.
   */
	*(uint32_t*)(loader + loader_entry_offs) = (uint32_t)(ehdr.e_entry & ADDRESS_MASK);

	/* Calculate the loader checksum */
	cksum = 0;
	for (len = 0; len < loader_size; len++) {
		ch = (uint8_t)loader[len]; cksum += ch;
	}
	i32 = start;       WRITE(&i32, sizeof(uint32_t));
	i32 = loader_size; WRITE(&i32, sizeof(uint32_t));
	i32 = cksum;       WRITE(&i32, sizeof(uint32_t));

	/* Put the loader in place */
	for (len = 0; len < loader_size; len++) {
		putc(loader[len], fout);
	}

	/*
	 * Now, process all PHDR's in the ELF file. We only care about stuff
	 * that must be LOAD-ed, since that's what bootloaders do as well...
	 */
	if (elf_getphnum(e, &n) == 0)
		errx(1, "elf_getphnum() failed: %s", elf_errmsg(-1));
	for (i = 0; i < n; i++) {
		if (gelf_getphdr(e, i, &phdr) != &phdr)
			errx(1, "elf_getphdr() failed: %s", elf_errmsg(-1));
		if (verbose > 1) {
			printf("PHDR %u\n", i);
			printf("  type    %d [", phdr.p_type);
			switch(phdr.p_type) {
				case PT_NULL: printf ("NULL"); break;
				case PT_LOAD: printf ("LOAD"); break;
				case PT_INTERP: printf ("INTERP"); break;
				case PT_NOTE: printf ("NOTE"); break;
				case PT_PHDR: printf ("PHDR"); break;
				case PT_TLS: printf ("TLS"); break;
				defaul: printf ("?"); break;
			}
			printf("]\n");
			printf("  offset  0x%jx\n", phdr.p_offset);
			printf("  vaddr   0x%jx\n", phdr.p_vaddr);
			printf("  paddr   0x%jx\n", phdr.p_paddr);
			printf("  filesz  0x%jx\n", phdr.p_filesz);
			printf("  memsz   0x%jx\n", phdr.p_memsz);
			printf("  flags   0x%jx\n", phdr.p_flags);
		}
		if (phdr.p_type != PT_LOAD) {
			if (verbose)
				printf("Skipping non-LOAD phdr %u\n", i);
			continue;
		}

		/*
		 * Write the loading address, size and a dummy checksum. We
		 * will fix the checksum later on once we know it.
		 */
		i32 = phdr.p_paddr & ADDRESS_MASK;  WRITE(&i32, sizeof(uint32_t));
		i32 = phdr.p_filesz; WRITE(&i32, sizeof(uint32_t));
		i32 = 0;             WRITE(&i32, sizeof(uint32_t));

		/*
		 * Copy the file over, and calculate the checksum (which is just
		 * an uint32_t with the sum of all bytes) while doing so.
		 */
		fseek(kfile, phdr.p_offset, SEEK_SET);
		cksum = 0; len = phdr.p_filesz;
		while (len--) {
			ch = getc(kfile);
			cksum += ch;
			putc(ch, fout);
		}

		/* Go back, fix the offset and pretend nothing happened */
		fseek(fout, -(phdr.p_filesz + sizeof(uint32_t)), SEEK_CUR);
		WRITE(&cksum, sizeof(cksum));
		fseek(fout,  phdr.p_filesz, SEEK_CUR);

		if (verbose)
			printf("section %u, addr %08x, size %08x, checksum %08x\n",
				i, (uint32_t)phdr.p_paddr, (uint32_t)phdr.p_filesz, cksum);
	}

	/*
   * Write NK.BIN footer - this is just a series of 3x uint32_t indicating
	 * a file of zero length, starting at our init code.
	 */
	i32 = 0;       WRITE(&i32, sizeof(i32));
	i32 = start;   WRITE(&i32, sizeof(i32));
	i32 = 0;       WRITE(&i32, sizeof(i32));

	elf_end(e);
	free(loader);
	fclose(kfile);
	fclose(fout);
	return 0;
}

/* vim:set ts=2 sw=2: */
