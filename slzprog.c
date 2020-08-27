// http://www.hanshq.net/making-executables.html

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// https://marcin-chwedczuk.github.io/a-closer-look-at-portable-executable-msdos-stub
// http://www.ctyme.com/intr/rb-0088.htm
// http://www.ctyme.com/intr/rb-0210.htm
const uint8_t dos_program[] = {
	0x0e,             // push cs
	0x07,             // pop es
	0xb4, 0x03,       // mov ah, 0x03
	0xb7, 0x00,       // mov bh, 0x00
	0xcd, 0x10,       // int 0x10
	0xb8, 0x03, 0x13, // mov ax, 0x1303
	0x31, 0xdb,       // xor bx, bx
	0xb9, 0x2a, 0x00, // mov cx, 42
	0xbd, 0x1a, 0x00, // mov bp, 0x1a
	0xcd, 0x10,       // int 0x10
	0xb8, 0x01, 0x4c, // mov ax, 0x4c01
	0xcd, 0x21,       // int 0x21
	'T', 0x0c, 'h', 0x0e, 'i', 0x0a, 's', 0x0b,
	' ', 0x09, 'p', 0x0d, 'r', 0x0c, 'o', 0x0e,
	'g', 0x0a, 'r', 0x0b, 'a', 0x09, 'm', 0x0d,
	' ', 0x0c, 'c', 0x0e, 'a', 0x0a, 'n', 0x0b,
	'n', 0x09, 'o', 0x0d, 't', 0x0c, ' ', 0x0e,
	'b', 0x0a, 'e', 0x0b, ' ', 0x09, 'r', 0x0b,
	'u', 0x0c, 'n', 0x0e, ' ', 0x0a, 'i', 0x0b,
	'n', 0x09, ' ', 0x0d, 'D', 0x0c, 'O', 0x0e,
	'S', 0x0a, ' ', 0x0b, 'm', 0x09, 'o', 0x0d,
	'd', 0x0c, 'e', 0x0e, '.', 0x0a,
	'\r', 0x0b, '\n', 0x09, '\a', 0x0d,
};

void write8(FILE *f, uint8_t value) {
	if (fputc(value, f) == EOF) {
		perror("fputc");
		exit(1);
	}
}

void write16(FILE *f, uint16_t x) {
	write8(f, x & 0xff);
	write8(f, (x >> 8) & 0xff);
}

void write32(FILE *f, uint32_t x) {
	write8(f, x & 0xff);
	write8(f, (x >> 8) & 0xff);
	write8(f, (x >> 16) & 0xff);
	write8(f, (x >> 24) & 0xff);
}

/* Write a string to file, padded to 8 bytes. */
void writestr8(FILE *file, const char *str) {
	size_t len, i;

	len = strlen(str);
	assert(len <= 8 && "The string must fit in 8 bytes.");

	for (i = 0; i < 8; i++) {
		write8(file, i < len ? str[i] : 0);
	}
}

/* Write a string to file, null-terminated and padded to 20 bytes. */
void writestr20(FILE *file, const char *str) {
	size_t len, i;

	len = strlen(str);
	assert(len <= 19 && "The string and terminator must fit in 20 bytes.");

	for (i = 0; i < 20; i++) {
		write8(file, i < len ? str[i] : 0);
	}
}

/* Seek to offset in file. */
void seek(FILE *file, long offset) {
	if (fseek(file, offset, SEEK_SET) == -1) {
		perror("fseek");
		exit(1);
	}
}

uint32_t align_to(uint32_t value, uint32_t alignment) {
	value += alignment - 1;
	value -= value % alignment;
	return value;
}

size_t read_file(const char* filename, void** ptr) {
	FILE* f = fopen(filename, "rb");
	if (!f) {
		perror("fopen");
		exit(1);
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		perror("fseek");
		exit(1);
	}
	long sz = ftell(f);
	if (sz == -1L) {
		perror("ftell");
		exit(1);
	}
	if (fseek(f, 0, SEEK_SET) != 0) {
		perror("fseek");
		exit(1);
	}
	if (!(*ptr = malloc(sz))) {
		perror("malloc");
		exit(1);
	}
	if (fread(*ptr, 1, sz, f) != sz) {
		perror("fread");
		exit(1);
	}
	if (fclose(f) == EOF) {
		perror("fclose");
		exit(1);
	}
	return sz;
}

uint32_t rsrc_rva, rsrc_offset, rsrc_sz;
struct {
	size_t entry_offset;
	size_t number_of_entries;
	size_t number_of_entries_so_far;
} rsrc_stack[100];
size_t rsrc_stack_top;
size_t rsrc_dir_end;
size_t rsrc_dir_sz;
size_t rsrc_data_end;

void rsrc_begin_directory(FILE* f, size_t number_of_entries) {
	fseek(f, rsrc_dir_end, SEEK_SET);
	rsrc_dir_end += 16 + 8 * number_of_entries;
	assert(rsrc_dir_end <= rsrc_offset + rsrc_dir_sz);
	write32(f, 0); // unused Characteristics
	write32(f, 0); // unused TimeDateStamp
	write32(f, 0); // unused MajorVersion and MinorVersion
	write16(f, 0); // NumberOfNamedEntries
	write16(f, number_of_entries); // NumberOfIdEntries
	rsrc_stack[rsrc_stack_top].entry_offset = ftell(f);
	rsrc_stack[rsrc_stack_top].number_of_entries = number_of_entries;
	rsrc_stack[rsrc_stack_top].number_of_entries_so_far = 0;
	rsrc_stack_top++;
}

void rsrc_end_directory(FILE* f) {
	rsrc_stack_top--;
	assert(rsrc_stack[rsrc_stack_top].number_of_entries == rsrc_stack[rsrc_stack_top].number_of_entries_so_far);
}

void rsrc_add_directory_entry(FILE* f, uint16_t id) {
	seek(f, rsrc_stack[rsrc_stack_top - 1].entry_offset);
	rsrc_stack[rsrc_stack_top - 1].entry_offset += 8;
	rsrc_stack[rsrc_stack_top - 1].number_of_entries_so_far++;
	write32(f, id); // Name
	// Assume the child to be a directory. If it is really data, this field will be set again in rsrc_begin_data().
	write32(f, 0x80000000 | (rsrc_dir_end - rsrc_offset)); // OffsetToData
}

size_t rsrc_data_begin;
void rsrc_begin_data(FILE* f) {
	assert(!rsrc_data_begin);
	fseek(f, -4, SEEK_CUR);
	rsrc_dir_end = align_to(rsrc_dir_end, 4);
	write32(f, rsrc_dir_end - rsrc_offset);
	seek(f, rsrc_dir_end);
	rsrc_dir_end += 16;
	write32(f, rsrc_rva + (rsrc_data_end - rsrc_offset)); // data RVA
	write32(f, 0x55aa); // Size (to be determined)
	write32(f, 0); // CodePage
	write32(f, 0); // Reserved
	write32(f, rsrc_data_end - rsrc_offset);
	seek(f, rsrc_data_end);
	rsrc_data_begin = rsrc_data_end;
}

void rsrc_end_data(FILE* f) {
	assert(rsrc_data_begin);
	rsrc_data_end = ftell(f);
	assert(rsrc_data_end <= rsrc_offset + rsrc_sz);
	seek(f, rsrc_dir_end - 12);
	write32(f, rsrc_data_end - rsrc_data_begin);
	rsrc_data_begin = 0;
}

void rsrc_write_string_table(
	FILE* f,
	const wchar_t* str0,
	const wchar_t* str1,
	const wchar_t* str2,
	const wchar_t* str3,
	const wchar_t* str4,
	const wchar_t* str5,
	const wchar_t* str6,
	const wchar_t* str7,
	const wchar_t* str8,
	const wchar_t* str9,
	const wchar_t* str10,
	const wchar_t* str11,
	const wchar_t* str12,
	const wchar_t* str13,
	const wchar_t* str14,
	const wchar_t* str15
) {
	write16(f, wcslen(str0)); fputws(str0, f);
	write16(f, wcslen(str1)); fputws(str1, f);
	write16(f, wcslen(str2)); fputws(str2, f);
	write16(f, wcslen(str3)); fputws(str3, f);
	write16(f, wcslen(str4)); fputws(str4, f);
	write16(f, wcslen(str5)); fputws(str5, f);
	write16(f, wcslen(str6)); fputws(str6, f);
	write16(f, wcslen(str7)); fputws(str7, f);
	write16(f, wcslen(str8)); fputws(str8, f);
	write16(f, wcslen(str9)); fputws(str9, f);
	write16(f, wcslen(str10)); fputws(str10, f);
	write16(f, wcslen(str11)); fputws(str11, f);
	write16(f, wcslen(str12)); fputws(str12, f);
	write16(f, wcslen(str13)); fputws(str13, f);
	write16(f, wcslen(str14)); fputws(str14, f);
	write16(f, wcslen(str15)); fputws(str15, f);
}

#define IAT_ENTRY_SZ               0x4
#define IMPORT_DIR_ENTRY_SZ        0x14
#define IMPORT_LOOKUP_TBL_ENTRY_SZ IAT_ENTRY_SZ
#define NAME_TABLE_ENTRY_SZ        22

#define IMAGE_BASE 0x00400000
#define SEC_ALIGN  0x1000
#define FILE_ALIGN 0x200

int main(int argc, char **argv) {
	FILE *f;
	size_t i;
	uint8_t* windows_program;
	size_t windows_program_sz = read_file("something.bin", (void**) &windows_program);

	f = fopen("slzprog-output.exe", "wb");
	if (!f) {
		perror("fopen");
		return 1;
	}

	uint32_t dos_stub_sz = 0x40 + sizeof(dos_program);
	uint32_t pe_offset = align_to(dos_stub_sz, 8);

	// On which fields are (un)used:
	// http://www.phreedom.org/research/tinype/
	write8(f, 'M');                /* Magic number (2 bytes) */
	write8(f, 'Z');
	write16(f, dos_stub_sz % 512); /* Last page size */
	write16(f, align_to(dos_stub_sz, 512) / 512); /* Pages in file */
	write16(f, 0);                 /* Relocations */
	write16(f, 4);                 /* Size of header in paragraphs */
	write16(f, 0);                 /* Minimum extra paragraphs needed */
	write16(f, 1);                 /* Maximum extra paragraphs needed */
	write16(f, 0);                 /* Initial (relative) SS value */
	write16(f, 0);                 /* Initial SP value */
	write16(f, 0);                 /* Rarely checked checksum */
	write16(f, 0);                 /* Initial IP value */
	write16(f, 0);                 /* Initial (relative) CS value */
	write16(f, 0x40);              /* File address of relocation table */
	write16(f, 0);                 /* Overlay number */
	for (i = 0; i < 4; i++)
		write16(f, 0);               /* Reserved (4 words) */
	write16(f, 0);                 /* OEM id */
	write16(f, 0);                 /* OEM info */
	for (i = 0; i < 10; i++)
		write16(f, 0);               /* Reserved (10 words) */
	write32(f, pe_offset);         /* File offset of PE header. */

	// DOS program.
	for (i = 0; i < sizeof(dos_program); i++) {
		write8(f, dos_program[i]);
	}

	/* PE signature. */
	seek(f, pe_offset);
	write8(f, 'P');
	write8(f, 'E');
	write8(f, 0);
	write8(f, 0);

	uint32_t num_sections = 4;

	/* COFF header. */
	write16(f, 0x14c); /* Machine: IMAGE_FILE_MACHINE_I386 */
	write16(f, num_sections); /* NumberOfSections */
	write32(f, 0); /* TimeDateStamp */
	write32(f, 0); /* PointerToSymbolTable */
	write32(f, 0); /* NumberOfSymbols */
	write16(f, 0xe0); /* SizeOfOptionalHeader */
	write16(f, 0x103); /* Characteristics: no relocations, exec, 32-bit */


	uint32_t headers_sz = pe_offset + 0xf8 + num_sections * 0x28;

	uint32_t idata_rva = align_to(headers_sz, SEC_ALIGN);
	uint32_t idata_offset = align_to(headers_sz, FILE_ALIGN);
	uint32_t num_imports = 12;
	uint32_t iat_rva = idata_rva;
	uint32_t iat_sz = (num_imports + 1) * IAT_ENTRY_SZ;
	uint32_t import_dir_table_rva = iat_rva + iat_sz;
	uint32_t import_dir_table_sz = 2 * IMPORT_DIR_ENTRY_SZ;
	uint32_t import_lookup_table_rva = import_dir_table_rva + import_dir_table_sz;
	uint32_t name_table_rva = import_lookup_table_rva + (num_imports + 1) * IMPORT_LOOKUP_TBL_ENTRY_SZ;
	uint32_t dll_name_rva = name_table_rva + num_imports * NAME_TABLE_ENTRY_SZ;
	uint32_t name_table_sz = num_imports * NAME_TABLE_ENTRY_SZ + 16;
	uint32_t idata_sz = name_table_rva + name_table_sz - idata_rva;

	uint32_t bss_rva = align_to(idata_rva + idata_sz, SEC_ALIGN);
	uint32_t bss_sz = 4096;

	uint32_t text_rva = align_to(bss_rva + bss_sz, SEC_ALIGN);
	uint32_t text_offset = align_to(idata_offset + idata_sz, FILE_ALIGN);
	uint32_t text_sz = windows_program_sz;

	rsrc_rva = align_to(text_rva + text_sz, SEC_ALIGN);
	rsrc_offset = align_to(text_offset + text_sz, FILE_ALIGN);
	rsrc_sz = 4096;
	rsrc_dir_sz = 512;

	/* Optional header, part 1: standard fields */
	write16(f, 0x10b); /* Magic: PE32 */
	write8(f, 0); /* MajorLinkerVersion */
	write8(f, 0); /* MinorLinkerVersion */
	write32(f, text_sz); /* SizeOfCode */
	write32(f, rsrc_sz + idata_sz); /* SizeOfInitializedData */
	write32(f, bss_sz); /* SizeOfUninitializedData */
	write32(f, text_rva); /* AddressOfEntryPoint */
	write32(f, text_rva); /* BaseOfCode */
	write32(f, idata_rva); /* BaseOfData */

	/* Optional header, part 2: Windows-specific fields */
	write32(f, IMAGE_BASE); /* ImageBase */
	write32(f, SEC_ALIGN); /* SectionAlignment */
	write32(f, FILE_ALIGN); /* FileAlignment */
	write16(f, 3); /* MajorOperatingSystemVersion */
	write16(f, 10); /* MinorOperatingSystemVersion */
	write16(f, 0); /* MajorImageVersion */
	write16(f, 0); /* MinorImageVersion */
	write16(f, 3); /* MajorSubsystemVersion */
	write16(f, 10); /* MinorSubsystemVersion */
	write32(f, 0); /* Win32VersionValue*/
	write32(f, align_to(rsrc_rva + rsrc_sz, SEC_ALIGN)); /* SizeOfImage */
	write32(f, align_to(headers_sz, FILE_ALIGN)); /* SizeOfHeaders */
	write32(f, 0); /* Checksum */
	write16(f, 3); /* Subsystem: Console */
	write16(f, 0); /* DllCharacteristics */
	write32(f, 0x100000); /* SizeOfStackReserve */
	write32(f, 0x1000); /* SizeOfStackCommit */
	write32(f, 0x100000); /* SizeOfHeapReserve */
	write32(f, 0x1000); /* SizeOfHeapCommit */
	write32(f, 0); /* LoadFlags */
	write32(f, 16); /* NumberOfRvaAndSizes */

	/* Optional header, part 3: data directories. */
	write32(f, 0); write32(f, 0); /* Export Table. */
	write32(f, import_dir_table_rva); /* Import Table. */
	write32(f, import_dir_table_sz);
	write32(f, rsrc_rva); write32(f, rsrc_sz); /* Resource Table. */
	write32(f, 0); write32(f, 0); /* Exception Table. */
	write32(f, 0); write32(f, 0); /* Certificate Table. */
	write32(f, 0); write32(f, 0); /* Base Relocation Table. */
	write32(f, 0); write32(f, 0); /* Debug. */
	write32(f, 0); write32(f, 0); /* Architecture. */
	write32(f, 0); write32(f, 0); /* Global Ptr. */
	write32(f, 0); write32(f, 0); /* TLS Table. */
	write32(f, 0); write32(f, 0); /* Load Config Table. */
	write32(f, 0); write32(f, 0); /* Bound Import. */
	write32(f, iat_rva); /* Import Address Table. */
	write32(f, iat_sz);
	write32(f, 0); write32(f, 0); /* Delay Import Descriptor. */
	write32(f, 0); write32(f, 0); /* CLR Runtime Header. */
	write32(f, 0); write32(f, 0); /* (Reserved). */

	// Section header #1: .idata
	writestr8(f, ".idata"); /* Name */
	write32(f, idata_sz); /* VirtualSize */
	write32(f, idata_rva); /* VirtualAddress */
	write32(f, align_to(idata_sz, FILE_ALIGN)); /* SizeOfRawData */
	write32(f, idata_offset); /* PointerToRawData */
	write32(f, 0); /* PointerToRelocations */
	write32(f, 0); /* PointerToLinenumbers */
	write16(f, 0); /* NumberOfRelocations */
	write16(f, 0); /* NumberOfLinenumbers */
	write32(f, 0xc0000040); /* Characteristics: data, read, write */

	// Section header #2: .bss
	writestr8(f, ".bss"); /* Name */
	write32(f, bss_sz); /* VirtualSize */
	write32(f, bss_rva); /* VirtualAddress */
	write32(f, 0); /* SizeOfRawData */
	write32(f, 0); /* PointerToRawData */
	write32(f, 0); /* PointerToRelocations */
	write32(f, 0); /* PointerToLinenumbers */
	write16(f, 0); /* NumberOfRelocations */
	write16(f, 0); /* NumberOfLinenumbers */
	write32(f, 0xc0000080); /* Characteristics: uninit. data, read, write */

	// Section header #3: .text
	writestr8(f, ".text"); /* Name */
	write32(f, text_sz); /* VirtualSize */
	write32(f, text_rva); /* VirtualAddress */
	write32(f, align_to(text_sz, FILE_ALIGN)); /* SizeOfRawData */
	write32(f, text_offset); /* PointerToRawData */
	write32(f, 0); /* PointerToRelocations */
	write32(f, 0); /* PointerToLinenumbers */
	write16(f, 0); /* NumberOfRelocations */
	write16(f, 0); /* NumberOfLinenumbers */
	write32(f, 0x60000020); /* Characteristics: code, read, execute */

	// Section header #4: .rsrc
	writestr8(f, ".rsrc"); /* Name */
	write32(f, rsrc_sz); /* VirtualSize */
	write32(f, rsrc_rva); /* VirtualAddress */
	write32(f, align_to(rsrc_sz, FILE_ALIGN)); /* SizeOfRawData */
	write32(f, rsrc_offset); /* PointerToRawData */
	write32(f, 0); /* PointerToRelocations */
	write32(f, 0); /* PointerToLinenumbers */
	write16(f, 0); /* NumberOfRelocations */
	write16(f, 0); /* NumberOfLinenumbers */
	write32(f, 0x40000040); /* Characteristics: data, read */


	/* Write .idata segment. */
	seek(f, idata_offset);

	/* Import Address Table (IAT):
	   (Same as the Import Lookup Table) */
	for (i = 0; i < num_imports; i++) {
		write32(f, name_table_rva + i * NAME_TABLE_ENTRY_SZ);
	}
	write32(f, 0);

	/* Import Directory Table */
	/* kernel32.dll */
	write32(f, import_lookup_table_rva); /* ILT RVA */
	write32(f, 0); /* Time/Data Stamp */
	write32(f, 0); /* Forwarder Chain */
	write32(f, dll_name_rva); /* Name RVA */
	write32(f, iat_rva); /* Import Address Table RVA */
	/* Null terminator */
	write32(f, 0);
	write32(f, 0);
	write32(f, 0);
	write32(f, 0);
	write32(f, 0);

	/* Import Lookup Table */
	for (i = 0; i < num_imports; i++) {
		write32(f, name_table_rva + i * NAME_TABLE_ENTRY_SZ);
	}
	write32(f, 0);

	/* Hint/Name Table */
	write16(f, 0); writestr20(f, "ExitProcess");
	write16(f, 0); writestr20(f, "GetLastError");
	write16(f, 0); writestr20(f, "LoadLibraryExA");
	write16(f, 0); writestr20(f, "GetProcAddress");
	write16(f, 0); writestr20(f, "FreeLibrary");
	write16(f, 0); writestr20(f, "GetStdHandle");
	write16(f, 0); writestr20(f, "ReadFile");
	write16(f, 0); writestr20(f, "WriteFile");
	write16(f, 0); writestr20(f, "OutputDebugStringA");
	write16(f, 0); writestr20(f, "HeapAlloc");
	write16(f, 0); writestr20(f, "GetProcessHeap");
	write16(f, 0); writestr20(f, "HeapFree");
	/* Put the dll name here too; we've got to write it somewhere. */
	writestr20(f, "kernel32.dll");

	// Write .text segment.
	seek(f, text_offset);
	for (i = 0; i < windows_program_sz; i++) {
		write8(f, windows_program[i]);
	}

	// Write .rsrc segment.
	seek(f, rsrc_offset);
	for (i = 0; i < rsrc_sz; i++) {
		write8(f, 0);
	}
	rsrc_dir_end = rsrc_offset;
	rsrc_data_end = rsrc_offset + rsrc_dir_sz;
	rsrc_begin_directory(f, 4);
		rsrc_add_directory_entry(f, 16);
		rsrc_begin_directory(f, 1);
			rsrc_add_directory_entry(f, 1);
			rsrc_begin_directory(f, 1);
				rsrc_add_directory_entry(f, 2052);
				rsrc_begin_data(f);
					// Version info goes here.
				rsrc_end_data(f);
			rsrc_end_directory(f);
		rsrc_end_directory(f);
		rsrc_add_directory_entry(f, 6);
		rsrc_begin_directory(f, 1);
			rsrc_add_directory_entry(f, 1);
			rsrc_begin_directory(f, 3);
				// 字符串表中的字符一般不是零结尾的，但是只要一个隔一个放，中间为空字符串计数的零就有用了。
				rsrc_add_directory_entry(f, 1033);
				rsrc_begin_data(f);
					rsrc_write_string_table(f, L"Grass.", L"", L"More grass.", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"");
				rsrc_end_data(f);
				rsrc_add_directory_entry(f, 1041);
				rsrc_begin_data(f);
					rsrc_write_string_table(f, L"くさ。", L"", L"もっとくさ。", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"");
				rsrc_end_data(f);
				rsrc_add_directory_entry(f, 2052);
				rsrc_begin_data(f);
					rsrc_write_string_table(f, L"草。", L"", L"更草。", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"", L"");
				rsrc_end_data(f);
			rsrc_end_directory(f);
		rsrc_end_directory(f);
		rsrc_add_directory_entry(f, 10);
		rsrc_begin_directory(f, 1);
			rsrc_add_directory_entry(f, 1);
			rsrc_begin_directory(f, 1);
				rsrc_add_directory_entry(f, 0);
				rsrc_begin_data(f);
					for (i = 0; i < 256; i++) write8(f, i);
				rsrc_end_data(f);
			rsrc_end_directory(f);
		rsrc_end_directory(f);
		rsrc_add_directory_entry(f, 24);
		rsrc_begin_directory(f, 1);
			rsrc_add_directory_entry(f, 1);
			rsrc_begin_directory(f, 1);
				rsrc_add_directory_entry(f, 0);
				rsrc_begin_data(f);
					fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>");
					fprintf(f, "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">");
					fprintf(f, "<compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\"><application>");
					fprintf(f,
						"<supportedOS Id=\"{e2011457-1546-43c5-a5fe-008deee3d3f0}\"/>" // Windows Vista
						"<supportedOS Id=\"{35138b9a-5d96-4fbd-8e2d-a2440225f93a}\"/>" // Windows 7
						"<supportedOS Id=\"{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}\"/>" // Windows 8
						"<supportedOS Id=\"{1f676c76-80e1-4239-95bb-83d0f6d0da78}\"/>" // Windows 8.1
						"<supportedOS Id=\"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}\"/>" // Windows 10
					);
					fprintf(f, "</application></compatibility>");
					fprintf(f, "<dependency><dependentAssembly><assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" version=\"6.0.0.0\" processorArchitecture=\"*\" publicKeyToken=\"6595b64144ccf1df\" language=\"*\" /></dependentAssembly></dependency>");
					fprintf(f, "<application xmlns=\"urn:schemas-microsoft-com:asm.v3\"><windowsSettings><dpiAware xmlns=\"http://schemas.microsoft.com/SMI/2005/WindowsSettings\">true</dpiAware><dpiAwareness xmlns=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\">PerMonitorV2</dpiAwareness></windowsSettings></application>");
					fprintf(f, "<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v2\"><security><requestedPrivileges><requestedExecutionLevel level=\"asInvoker\" uiAccess=\"false\"/></requestedPrivileges></security></trustInfo>");
					fprintf(f, "</assembly>");
				rsrc_end_data(f);
			rsrc_end_directory(f);
		rsrc_end_directory(f);
	rsrc_end_directory(f);

	fseek(f, rsrc_offset + rsrc_sz, SEEK_SET);
	// VS_VERSIONINFO
	fprintf(f, "Data after this point is useless.");
  write16(f, 123 /*some*/); // wLength
  write16(f, 456 /*length*/); // wValueLength
  write16(f, 0); // wType
  write16(f, 'V'); // szKey
  write16(f, 'S');
  write16(f, '_');
  write16(f, 'V');
  write16(f, 'E');
  write16(f, 'R');
  write16(f, 'S');
  write16(f, 'I');
  write16(f, 'O');
  write16(f, 'N');
  write16(f, '_');
  write16(f, 'I');
  write16(f, 'N');
  write16(f, 'F');
  write16(f, 'O');
  write16(f, 0);
  write16(f, 0); // Padding1
	// VS_FIXEDFILEINFO
	write32(f, 0xfeef04bd); // dwSignature
  write32(f, 0x00010000); // dwStrucVersion
  write32(f, 0); // dwFileVersionMS
  write32(f, 0); // dwFileVersionLS
  write32(f, 0); // dwProductVersionMS
  write32(f, 0); // dwProductVersionLS
  write32(f, 0x00000010); // dwFileFlagsMask = VS_FF_INFOINFERRED
  write32(f, 0x00000000); // dwFileFlags
  write32(f, 0x00000004); // dwFileOS = VOS__WINDOWS32
  write32(f, 0x00000001); // dwFileType = VFT_APP
  write32(f, 0); // dwFileSubtype
  write32(f, 0); // dwFileDateMS
  write32(f, 0); // dwFileDateLS
  write16(f, 0); // Padding2
  write16(f, 0); // Children


	if (fclose(f) == EOF) {
		perror("fclose");
		return 1;
	}
	return 0;
}
