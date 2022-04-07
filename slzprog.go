// http://www.hanshq.net/making-executables.html

package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"os"
	"unicode/utf16"
)

// https://marcin-chwedczuk.github.io/a-closer-look-at-portable-executable-msdos-stub
// http://www.ctyme.com/intr/rb-0088.htm
// http://www.ctyme.com/intr/rb-0210.htm
var dos_program = []byte{
	0x0e,       // push cs
	0x07,       // pop es
	0xb4, 0x03, // mov ah, 0x03
	0xb7, 0x00, // mov bh, 0x00
	0xcd, 0x10, // int 0x10
	0xb8, 0x03, 0x13, // mov ax, 0x1303
	0x31, 0xdb, // xor bx, bx
	0xb9, 0x2a, 0x00, // mov cx, 42
	0xbd, 0x1a, 0x00, // mov bp, 0x1a
	0xcd, 0x10, // int 0x10
	0xb8, 0x01, 0x4c, // mov ax, 0x4c01
	0xcd, 0x21, // int 0x21
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
}

var rsrc_rva, rsrc_offset, rsrc_sz int64

type RSRCDirectoryEntry struct {
	ID uint16
	// An entry can either be a directory or data.
	IsDirectory bool
	Directory   []RSRCDirectoryEntry
	Data        []byte
}

func rsrc_add_directory_entry(f *os.File, entry *RSRCDirectoryEntry) {
	if entry.IsDirectory {
		write32(f, 0)                            // unused Characteristics
		write32(f, 0)                            // unused TimeDateStamp
		write32(f, 0)                            // unused MajorVersion and MinorVersion
		write16(f, 0)                            // NumberOfNamedEntries
		write16(f, uint16(len(entry.Directory))) // NumberOfIdEntries
		// Reserve space for entry metadata.
		entries_begin, _ := f.Seek(0, io.SeekCurrent)
		f.Seek(int64(len(entry.Directory))*8, io.SeekCurrent)
		for i, subentry := range entry.Directory {
			// Append the entry.
			subentry_begin, _ := f.Seek(0, io.SeekCurrent)
			rsrc_add_directory_entry(f, &subentry)
			subentry_end, _ := f.Seek(0, io.SeekCurrent)
			// Fill in the metadata.
			f.Seek(entries_begin+int64(i)*8, io.SeekStart)
			offset := uint32(subentry_begin - rsrc_offset)
			if subentry.IsDirectory {
				offset |= 0x80000000
			}
			write32(f, uint32(subentry.ID)) // Name
			write32(f, offset)              // OffsetToData
			f.Seek(align_to(subentry_end, 4), io.SeekStart)
		}
	} else {
		subentry_begin, _ := f.Seek(0, io.SeekCurrent)
		write32(f, uint32(rsrc_rva+(subentry_begin+16-rsrc_offset))) // data RVA
		write32(f, uint32(len(entry.Data)))                          // Size
		write32(f, 0)                                                // CodePage
		write32(f, 0)                                                // Reserved
		f.Write(entry.Data)
	}
}

func rsrc_write_string_table(str []string) []byte {
	assert(len(str) <= 16)
	var f bytes.Buffer
	for _, str := range str {
		str := utf16.Encode([]rune(str))
		write16(&f, uint16(len(str)))
		binary.Write(&f, binary.LittleEndian, str)
	}
	for i := len(str); i < 16; i++ {
		write16(&f, 0)
	}
	return f.Bytes()
}

const IAT_ENTRY_SZ = 0x4
const IMPORT_DIR_ENTRY_SZ = 0x14
const IMPORT_LOOKUP_TBL_ENTRY_SZ = IAT_ENTRY_SZ
const NAME_TABLE_ENTRY_SZ = 22

const IMAGE_BASE = 0x00400000
const SEC_ALIGN = 0x1000
const FILE_ALIGN = 0x200

func main() {
	windows_program, _ := os.ReadFile("something.bin")

	f, err := os.Create("slzprog-output.exe")
	if err != nil {
		fmt.Printf("os.Create: %v\n", err)
		os.Exit(1)
	}

	dos_stub_sz := 0x40 + int64(len(dos_program))
	pe_offset := align_to(dos_stub_sz, 8)

	// On which fields are (un)used:
	// http://www.phreedom.org/research/tinype/
	write8(f, 'M') /* Magic number (2 bytes) */
	write8(f, 'Z')
	write16(f, uint16(dos_stub_sz%512))                /* Last page size */
	write16(f, uint16(align_to(dos_stub_sz, 512)/512)) /* Pages in file */
	write16(f, 0)                                      /* Relocations */
	write16(f, 4)                                      /* Size of header in paragraphs */
	write16(f, 0)                                      /* Minimum extra paragraphs needed */
	write16(f, 1)                                      /* Maximum extra paragraphs needed */
	write16(f, 0)                                      /* Initial (relative) SS value */
	write16(f, 0)                                      /* Initial SP value */
	write16(f, 0)                                      /* Rarely checked checksum */
	write16(f, 0)                                      /* Initial IP value */
	write16(f, 0)                                      /* Initial (relative) CS value */
	write16(f, 0x40)                                   /* File address of relocation table */
	write16(f, 0)                                      /* Overlay number */
	for i := 0; i < 4; i++ {
		write16(f, 0) /* Reserved (4 words) */
	}
	write16(f, 0) /* OEM id */
	write16(f, 0) /* OEM info */
	for i := 0; i < 10; i++ {
		write16(f, 0) /* Reserved (10 words) */
	}
	write32(f, uint32(pe_offset)) /* File offset of PE header. */

	// DOS program.
	f.Write(dos_program)

	// PE signature.
	f.Seek(pe_offset, io.SeekStart)
	writestrn(f, "PE", 4)

	num_sections := 4

	/* COFF header. */
	write16(f, 0x14c)                /* Machine: IMAGE_FILE_MACHINE_I386 */
	write16(f, uint16(num_sections)) /* NumberOfSections */
	write32(f, 0)                    /* TimeDateStamp */
	write32(f, 0)                    /* PointerToSymbolTable */
	write32(f, 0)                    /* NumberOfSymbols */
	write16(f, 0xe0)                 /* SizeOfOptionalHeader */
	write16(f, 0x103)                /* Characteristics: no relocations, exec, 32-bit */

	headers_sz := pe_offset + 0xf8 + int64(num_sections)*0x28

	idata_rva := align_to(headers_sz, SEC_ALIGN)
	idata_offset := align_to(headers_sz, FILE_ALIGN)
	num_imports := 12
	iat_rva := idata_rva
	iat_sz := (int64(num_imports) + 1) * IAT_ENTRY_SZ
	import_dir_table_rva := iat_rva + iat_sz
	import_dir_table_sz := int64(2 * IMPORT_DIR_ENTRY_SZ)
	import_lookup_table_rva := import_dir_table_rva + import_dir_table_sz
	name_table_rva := import_lookup_table_rva + (int64(num_imports)+1)*IMPORT_LOOKUP_TBL_ENTRY_SZ
	dll_name_rva := name_table_rva + int64(num_imports)*NAME_TABLE_ENTRY_SZ
	name_table_sz := int64(num_imports)*NAME_TABLE_ENTRY_SZ + 16
	idata_sz := name_table_rva + name_table_sz - idata_rva

	bss_rva := align_to(idata_rva+idata_sz, SEC_ALIGN)
	bss_sz := int64(4096)

	text_rva := align_to(bss_rva+bss_sz, SEC_ALIGN)
	text_offset := align_to(idata_offset+idata_sz, FILE_ALIGN)
	text_sz := int64(len(windows_program))

	rsrc_rva = align_to(text_rva+text_sz, SEC_ALIGN)
	rsrc_offset = align_to(text_offset+text_sz, FILE_ALIGN)
	rsrc_sz = 4096

	/* Optional header, part 1: standard fields */
	write16(f, 0x10b)                    /* Magic: PE32 */
	write8(f, 0)                         /* MajorLinkerVersion */
	write8(f, 0)                         /* MinorLinkerVersion */
	write32(f, uint32(text_sz))          /* SizeOfCode */
	write32(f, uint32(rsrc_sz+idata_sz)) /* SizeOfInitializedData */
	write32(f, uint32(bss_sz))           /* SizeOfUninitializedData */
	write32(f, uint32(text_rva))         /* AddressOfEntryPoint */
	write32(f, uint32(text_rva))         /* BaseOfCode */
	write32(f, uint32(idata_rva))        /* BaseOfData */

	/* Optional header, part 2: Windows-specific fields */
	write32(f, IMAGE_BASE)                                    /* ImageBase */
	write32(f, SEC_ALIGN)                                     /* SectionAlignment */
	write32(f, FILE_ALIGN)                                    /* FileAlignment */
	write16(f, 3)                                             /* MajorOperatingSystemVersion */
	write16(f, 10)                                            /* MinorOperatingSystemVersion */
	write16(f, 0)                                             /* MajorImageVersion */
	write16(f, 0)                                             /* MinorImageVersion */
	write16(f, 3)                                             /* MajorSubsystemVersion */
	write16(f, 10)                                            /* MinorSubsystemVersion */
	write32(f, 0)                                             /* Win32VersionValue*/
	write32(f, uint32(align_to(rsrc_rva+rsrc_sz, SEC_ALIGN))) /* SizeOfImage */
	write32(f, uint32(align_to(headers_sz, FILE_ALIGN)))      /* SizeOfHeaders */
	write32(f, 0)                                             /* Checksum */
	write16(f, 3)                                             /* Subsystem: Console */
	write16(f, 0)                                             /* DllCharacteristics */
	write32(f, 0x100000)                                      /* SizeOfStackReserve */
	write32(f, 0x1000)                                        /* SizeOfStackCommit */
	write32(f, 0x100000)                                      /* SizeOfHeapReserve */
	write32(f, 0x1000)                                        /* SizeOfHeapCommit */
	write32(f, 0)                                             /* LoadFlags */
	write32(f, 16)                                            /* NumberOfRvaAndSizes */

	/* Optional header, part 3: data directories. */
	write32(f, 0) // Export Table.
	write32(f, 0)
	write32(f, uint32(import_dir_table_rva)) // Import Table.
	write32(f, uint32(import_dir_table_sz))
	write32(f, uint32(rsrc_rva)) // Resource Table.
	write32(f, uint32(rsrc_sz))
	write32(f, 0) // Exception Table.
	write32(f, 0)
	write32(f, 0) // Certificate Table.
	write32(f, 0)
	write32(f, 0) // Base Relocation Table.
	write32(f, 0)
	write32(f, 0) // Debug.
	write32(f, 0)
	write32(f, 0) // Architecture.
	write32(f, 0)
	write32(f, 0) // Global Ptr.
	write32(f, 0)
	write32(f, 0) // TLS Table.
	write32(f, 0)
	write32(f, 0) // Load Config Table.
	write32(f, 0)
	write32(f, 0) // Bound Import.
	write32(f, 0)
	write32(f, uint32(iat_rva)) // Import Address Table.
	write32(f, uint32(iat_sz))
	write32(f, 0) // Delay Import Descriptor.
	write32(f, 0)
	write32(f, 0) // CLR Runtime Header.
	write32(f, 0)
	write32(f, 0) // (Reserved).
	write32(f, 0)

	// Section header #1: .idata
	writestrn(f, ".idata", 8)                          /* Name */
	write32(f, uint32(idata_sz))                       /* VirtualSize */
	write32(f, uint32(idata_rva))                      /* VirtualAddress */
	write32(f, uint32(align_to(idata_sz, FILE_ALIGN))) /* SizeOfRawData */
	write32(f, uint32(idata_offset))                   /* PointerToRawData */
	write32(f, 0)                                      /* PointerToRelocations */
	write32(f, 0)                                      /* PointerToLinenumbers */
	write16(f, 0)                                      /* NumberOfRelocations */
	write16(f, 0)                                      /* NumberOfLinenumbers */
	write32(f, 0xc0000040)                             /* Characteristics: data, read, write */

	// Section header #2: .bss
	writestrn(f, ".bss", 8)     /* Name */
	write32(f, uint32(bss_sz))  /* VirtualSize */
	write32(f, uint32(bss_rva)) /* VirtualAddress */
	write32(f, 0)               /* SizeOfRawData */
	write32(f, 0)               /* PointerToRawData */
	write32(f, 0)               /* PointerToRelocations */
	write32(f, 0)               /* PointerToLinenumbers */
	write16(f, 0)               /* NumberOfRelocations */
	write16(f, 0)               /* NumberOfLinenumbers */
	write32(f, 0xc0000080)      /* Characteristics: uninit. data, read, write */

	// Section header #3: .text
	writestrn(f, ".text", 8)                          /* Name */
	write32(f, uint32(text_sz))                       /* VirtualSize */
	write32(f, uint32(text_rva))                      /* VirtualAddress */
	write32(f, uint32(align_to(text_sz, FILE_ALIGN))) /* SizeOfRawData */
	write32(f, uint32(text_offset))                   /* PointerToRawData */
	write32(f, 0)                                     /* PointerToRelocations */
	write32(f, 0)                                     /* PointerToLinenumbers */
	write16(f, 0)                                     /* NumberOfRelocations */
	write16(f, 0)                                     /* NumberOfLinenumbers */
	write32(f, 0x60000020)                            /* Characteristics: code, read, execute */

	// Section header #4: .rsrc
	writestrn(f, ".rsrc", 8)                          /* Name */
	write32(f, uint32(rsrc_sz))                       /* VirtualSize */
	write32(f, uint32(rsrc_rva))                      /* VirtualAddress */
	write32(f, uint32(align_to(rsrc_sz, FILE_ALIGN))) /* SizeOfRawData */
	write32(f, uint32(rsrc_offset))                   /* PointerToRawData */
	write32(f, 0)                                     /* PointerToRelocations */
	write32(f, 0)                                     /* PointerToLinenumbers */
	write16(f, 0)                                     /* NumberOfRelocations */
	write16(f, 0)                                     /* NumberOfLinenumbers */
	write32(f, 0x40000040)                            /* Characteristics: data, read */

	/* Write .idata segment. */
	f.Seek(idata_offset, io.SeekStart)

	/* Import Address Table (IAT):
	   (Same as the Import Lookup Table) */
	for i := 0; i < num_imports; i++ {
		write32(f, uint32(name_table_rva+int64(i)*NAME_TABLE_ENTRY_SZ))
	}
	write32(f, 0)

	/* Import Directory Table */
	/* kernel32.dll */
	write32(f, uint32(import_lookup_table_rva)) /* ILT RVA */
	write32(f, 0)                               /* Time/Data Stamp */
	write32(f, 0)                               /* Forwarder Chain */
	write32(f, uint32(dll_name_rva))            /* Name RVA */
	write32(f, uint32(iat_rva))                 /* Import Address Table RVA */
	/* Null terminator */
	write32(f, 0)
	write32(f, 0)
	write32(f, 0)
	write32(f, 0)
	write32(f, 0)

	/* Import Lookup Table */
	for i := 0; i < num_imports; i++ {
		write32(f, uint32(name_table_rva+int64(i)*NAME_TABLE_ENTRY_SZ))
	}
	write32(f, 0)

	// Hint/Name Table
	for _, name := range []string{
		"ExitProcess",
		"GetLastError",
		"LoadLibraryExA",
		"GetProcAddress",
		"FreeLibrary",
		"GetStdHandle",
		"ReadFile",
		"WriteFile",
		"OutputDebugStringA",
		"HeapAlloc",
		"GetProcessHeap",
		"HeapFree",
	} {
		write16(f, 0)
		assert(len(name) <= 19)
		writestrn(f, name, 20)
	}
	// Put the dll name here too; we've got to write it somewhere.
	writestrn(f, "kernel32.dll", 20)

	// Write .text segment.
	f.Seek(text_offset, io.SeekStart)
	f.Write(windows_program)

	// Write .rsrc segment.
	f.Seek(rsrc_offset, io.SeekStart)
	for i := int64(0); i < rsrc_sz; i++ {
		write8(f, 0)
	}
	f.Seek(rsrc_offset, io.SeekStart)
	rsrc_add_directory_entry(f, &RSRCDirectoryEntry{IsDirectory: true, Directory: []RSRCDirectoryEntry{
		{ID: 16, IsDirectory: true, Directory: []RSRCDirectoryEntry{
			{ID: 1, IsDirectory: true, Directory: []RSRCDirectoryEntry{
				{ID: 2052, IsDirectory: false, Data: []byte{
					// Version info goes here.
				}},
			}},
		}},
		{ID: 6, IsDirectory: true, Directory: []RSRCDirectoryEntry{
			{ID: 1, IsDirectory: true, Directory: []RSRCDirectoryEntry{
				// 字符串表中的字符一般不是零结尾的，但是只要一个隔一个放，中间为空字符串计数的零就有用了。
				{ID: 1033, IsDirectory: false, Data: rsrc_write_string_table([]string{"Grass.", "", "More grass."})},
				{ID: 1041, IsDirectory: false, Data: rsrc_write_string_table([]string{"くさ。", "", "もっとくさ。"})},
				{ID: 2052, IsDirectory: false, Data: rsrc_write_string_table([]string{"草。", "", "更草。"})},
			}},
		}},
		{ID: 10, IsDirectory: true, Directory: []RSRCDirectoryEntry{
			{ID: 1, IsDirectory: true, Directory: []RSRCDirectoryEntry{
				{ID: 0, IsDirectory: false, Data: func() []byte {
					var b [256]byte
					for i := 0; i < 256; i++ {
						b[i] = byte(i)
					}
					return b[:]
				}()},
			}},
		}},
		{ID: 24, IsDirectory: true, Directory: []RSRCDirectoryEntry{
			{ID: 1, IsDirectory: true, Directory: []RSRCDirectoryEntry{
				{ID: 0, IsDirectory: false, Data: []byte(
					"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>" +
						"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">" +
						"<compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\"><application>" +
						"<supportedOS Id=\"{e2011457-1546-43c5-a5fe-008deee3d3f0}\"/>" + // Windows Vista
						"<supportedOS Id=\"{35138b9a-5d96-4fbd-8e2d-a2440225f93a}\"/>" + // Windows 7
						"<supportedOS Id=\"{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}\"/>" + // Windows 8
						"<supportedOS Id=\"{1f676c76-80e1-4239-95bb-83d0f6d0da78}\"/>" + // Windows 8.1
						"<supportedOS Id=\"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}\"/>" + // Windows 10
						"</application></compatibility>" +
						"<dependency><dependentAssembly><assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" version=\"6.0.0.0\" processorArchitecture=\"*\" publicKeyToken=\"6595b64144ccf1df\" language=\"*\" /></dependentAssembly></dependency>" +
						"<application xmlns=\"urn:schemas-microsoft-com:asm.v3\"><windowsSettings><dpiAware xmlns=\"http://schemas.microsoft.com/SMI/2005/WindowsSettings\">true</dpiAware><dpiAwareness xmlns=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\">PerMonitorV2</dpiAwareness></windowsSettings></application>" +
						"<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v2\"><security><requestedPrivileges><requestedExecutionLevel level=\"asInvoker\" uiAccess=\"false\"/></requestedPrivileges></security></trustInfo>" +
						"</assembly>",
				)},
			}},
		}},
	}})

	f.Seek(rsrc_offset+rsrc_sz, io.SeekStart)
	// VS_VERSIONINFO
	fmt.Fprintf(f, "Data after this point is useless.")
	write16(f, 123 /*some*/)   // wLength
	write16(f, 456 /*length*/) // wValueLength
	write16(f, 0)              // wType
	write16(f, 'V')            // szKey
	write16(f, 'S')
	write16(f, '_')
	write16(f, 'V')
	write16(f, 'E')
	write16(f, 'R')
	write16(f, 'S')
	write16(f, 'I')
	write16(f, 'O')
	write16(f, 'N')
	write16(f, '_')
	write16(f, 'I')
	write16(f, 'N')
	write16(f, 'F')
	write16(f, 'O')
	write16(f, 0)
	write16(f, 0) // Padding1
	// VS_FIXEDFILEINFO
	write32(f, 0xfeef04bd) // dwSignature
	write32(f, 0x00010000) // dwStrucVersion
	write32(f, 0)          // dwFileVersionMS
	write32(f, 0)          // dwFileVersionLS
	write32(f, 0)          // dwProductVersionMS
	write32(f, 0)          // dwProductVersionLS
	write32(f, 0x00000010) // dwFileFlagsMask = VS_FF_INFOINFERRED
	write32(f, 0x00000000) // dwFileFlags
	write32(f, 0x00000004) // dwFileOS = VOS__WINDOWS32
	write32(f, 0x00000001) // dwFileType = VFT_APP
	write32(f, 0)          // dwFileSubtype
	write32(f, 0)          // dwFileDateMS
	write32(f, 0)          // dwFileDateLS
	write16(f, 0)          // Padding2
	write16(f, 0)          // Children

	err = f.Close()
	if err != nil {
		fmt.Printf("Close: %v\n", err)
		os.Exit(1)
	}
}
