// http://www.hanshq.net/making-executables.html

package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"os"
	"os/exec"
	"unicode/utf16"
)

// https://marcin-chwedczuk.github.io/a-closer-look-at-portable-executable-msdos-stub
// https://justine.lol/ape.html
// General tips for bootloader development: https://stackoverflow.com/a/32705076
var header = [512]byte{
	// A executable header like the one in the αcτµαlly pδrταblε εxεcµταblε format.
	// The boot sector presented here does not try to jump as soon as possible with MZqFpD.
	// As a result, the DOS MZ loader (at least in DOSBox) is happy with this version.
	'M', // dec bp / Mark Zbikowski
	// In fact, the default stack pointer the BIOS sets may interfere with our code.
	// It's unavoidable, so we just assume it's safe to use this memory word.
	'Z', // pop dx / …
	// At least one lowercase letter is required before the first newline and the first newline must come before the first NUL for shells to recognize the file as a shell script.
	'f', '=', // cmp eax, 2599 / last page size
	'\'', '\n', // … / number of pages in file
	0, 0, // … / number of relocations
	4, 0, // add al, 0 / size of header in paragraphs
	0x0d, 0x00, // or ax, 0xff00 / minimum extra paragraphs needed
	0xff,       // … / maximum extra paragraphs needed
	0xe8,       // call $+15 / …
	0x0c, 0x00, // … / initial (relative) SS value
	0, 0, // initial SP value
	0, 0, // rarely checked checksum
	0, 0, // initial IP value
	0, 0, // initial (relative) CS value
	0, 0, // file address of relocation table
	0, 0, // overlay number
	0xfa, // cli
	// Int 10/AH=00h set video mode
	// http://www.ctyme.com/intr/rb-0069.htm
	0x31, 0xc0, // xor ax, ax
	0x8e, 0xd8, // mov ds, ax
	0x8e, 0xc0, // mov es, ax
	0xcd, 0x10, // int 0x10
	// Int 10/AH=13h write string
	// http://www.ctyme.com/intr/rb-0210.htm
	0xb8, 0x00, 0x13, // mov ax, 0x1300
	0xb9, 0x16, 0x00, // mov cx, 0x0016
	0xba, 0x09, 0x16, // mov dx, 0x1609
	0xbb, 0x3b, 0x00, // mov bx, 0x003b
	0xbd, 0xd2, 0x7c, // mov bp, 0x7cd2
	0xcd, 0x10, // int 0x10
	0xb8, 0x00, 0xb8, // mov ax, 0xb800
	0xfc,       // cld
	0xeb, 0x72, // jmp $+0x74
	0x00, 0x02, 0x00, 0x00, // file offset of pe header
	// The colorful DOS stub program.
	0x0e, // push cs
	0x07, // pop es
	// Int 10/AH=03h get cursor position and size
	// http://www.ctyme.com/intr/rb-0088.htm
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
	// The boot sector continues here.
	0x8e, 0xc0, // mov es, ax
	0xb4, 0x7c, // mov ah, 0x7c
	0x89, 0xc6, // mov si, ax
	0xfe, 0xc4, // inc ah
	0xbf, 238, 0, // mov di, 238
	0x83, 0xc7, 8, // 1: add di, 8
	0xb9, 16, 0, // 2: mov cx, 16
	0xa4,       // 3: movsb
	0x47,       // inc di
	0xe2, 0xfc, // loop 3b
	0x66, 0xaf, // scasd
	0x96,       // xchg si, ax
	0x39, 0xc6, // cmp si, ax
	0x73, 0xf2, // jae 2b
	0x3d, 0x00, 0x7e, // cmp ax, 0x7e00
	0x72, 0xea, // jb 1b
	0xf4,       // hlt
	0xeb, 0xfd, // jmp $-1
	' ', 'I', 'n', 'v', 'a', 'l', 'i', 'd',
	' ', 's', 'y', 's', 't', 'e', 'm',
	' ', 'd', 'i', 's', 'k', '.', ' ',
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x55, 0xaa,
}

type PESection struct {
	Name            string
	Characteristics uint32
	RVA             int64
	FilePos         int64
	Size            int64
}

var idata PESection

type IDataDirectoryEntry struct {
	LibraryName string // DLL name
	Symbols     []string
}

func IDataWrite(f io.Writer, entries []IDataDirectoryEntry) {
	// Calculate offsets for the the import address table and the hint/name table.
	iat_offset := (int64(len(entries)) + 1) * 20
	names_offset := iat_offset
	for _, entry := range entries {
		names_offset += (int64(len(entry.Symbols)) + 1) * 4
	}

	// DLL names and hint/name table entries, to be written after IDT and IAT.
	var names bytes.Buffer

	// IMAGE_IMPORT_DESCRIPTOR (import directory table)
	// ILT can be zero. The Delphi compiler creates such executables.
	// https://7shi.hateblo.jp/entry/2012/08/07/200241
	iat_end := iat_offset
	for _, entry := range entries {
		write32(f, 0)      // OriginalFirstThunk/Characteristics (import lookup table RVA)
		write32(f, 0)      // TimeDateStamp
		write32(f, 0)      // ForwarderChain
		write32(f, uint32( // Name
			int64(names.Len())+names_offset+idata.RVA))
		write32(f, uint32(iat_end+idata.RVA)) // FirstThunk (import address table RVA)
		names.WriteString(entry.LibraryName)
		names.WriteByte(0)
		iat_end += (int64(len(entry.Symbols)) + 1) * 4
	}
	assert(iat_end == names_offset)
	write32(f, 0)
	write32(f, 0)
	write32(f, 0)
	write32(f, 0)
	write32(f, 0)

	// IMAGE_THUNK_DATA32/PIMAGE_IMPORT_BY_NAME (import address table)
	for _, entry := range entries {
		for _, name := range entry.Symbols {
			if names.Len()%2 != 0 {
				names.WriteByte(0) // padding for Hint
			}
			write32(f, uint32(int64(names.Len())+names_offset+idata.RVA))
			write16(&names, 0)      // Hint
			names.WriteString(name) // Name
			names.WriteByte(0)
		}
		write32(f, 0)
	}

	// IMAGE_IMPORT_BY_NAME
	f.Write(names.Bytes())
}

var rsrc PESection

type RSRCDirectoryEntry struct {
	ID uint16
	// An entry can either be a directory or data.
	IsDirectory bool
	Directory   []RSRCDirectoryEntry
	Data        []byte
}

func LPCWSTR(str string) []byte {
	var f bytes.Buffer
	binary.Write(&f, binary.LittleEndian, utf16.Encode([]rune(str)))
	write16(&f, 0)
	return f.Bytes()
}

func RSRCWriteDirectoryEntry(f io.WriteSeeker, entry *RSRCDirectoryEntry) {
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
			RSRCWriteDirectoryEntry(f, &subentry)
			subentry_end, _ := f.Seek(0, io.SeekCurrent)
			// Fill in the metadata.
			f.Seek(entries_begin+int64(i)*8, io.SeekStart)
			offset := uint32(subentry_begin - rsrc.FilePos)
			if subentry.IsDirectory {
				offset |= 0x80000000
			}
			write32(f, uint32(subentry.ID)) // Name
			write32(f, offset)              // OffsetToData
			f.Seek(align_to(subentry_end, 4), io.SeekStart)
		}
	} else {
		subentry_begin, _ := f.Seek(0, io.SeekCurrent)
		write32(f, uint32(rsrc.RVA+(subentry_begin+16-rsrc.FilePos))) // data RVA
		write32(f, uint32(len(entry.Data)))                           // Size
		write32(f, 0)                                                 // CodePage
		write32(f, 0)                                                 // Reserved
		f.Write(entry.Data)
	}
}

func RSRCMakeStringTable(str []string) []byte {
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

func RSRCMakeFixedFileInfo(major, minor, build, revision uint16) []byte {
	// VS_FIXEDFILEINFO
	var f bytes.Buffer
	write32(&f, 0xfeef04bd) // dwSignature
	write32(&f, 0)          // dwStrucVersion
	write16(&f, minor)      // dwFileVersionMS
	write16(&f, major)      //
	write16(&f, revision)   // dwFileVersionLS
	write16(&f, build)      //
	write16(&f, minor)      // dwProductVersionMS
	write16(&f, major)      //
	write16(&f, revision)   // dwProductVersionLS
	write16(&f, build)      //
	write32(&f, 0)          // dwFileFlagsMask
	write32(&f, 0)          // dwFileFlags
	write32(&f, 0x00040004) // dwFileOS = VOS_NT_WINDOWS32
	write32(&f, 0x00000001) // dwFileType = VFT_APP
	write32(&f, 0)          // dwFileSubtype
	write32(&f, 0)          // dwFileDateMS
	write32(&f, 0)          // dwFileDateLS
	return f.Bytes()
}

// VS_VERSIONINFO, StringFileInfo, StringTable, String, VarFileInfo and Var all start with wLength, wValueLength, wType, szKey and Padding.
// https://docs.microsoft.com/en-us/windows/win32/menurc/version-information-structures
func RSRCMakeVersionInfoStruct(key string, valueIsText bool, value []byte, children [][]byte) []byte {
	var f bytes.Buffer
	write16(&f, 0x55aa) // wLength (to be determined)
	if valueIsText {
		write16(&f, uint16(len(value)/2)) // wValueLength
		write16(&f, 1)                    // wType
	} else {
		write16(&f, uint16(len(value))) // wValueLength
		write16(&f, 0)                  // wType
	}
	f.Write(LPCWSTR(key)) // szKey
	if f.Len()%4 != 0 {
		write16(&f, 0) // Padding1
	}
	f.Write(value) // Value
	for _, child := range children {
		if f.Len()%4 != 0 {
			write16(&f, 0) // Padding2
		}
		f.Write(child) // Children
	}
	b := f.Bytes()
	binary.LittleEndian.PutUint16(b, uint16(len(b))) // wLength
	return b
}

const SEC_ALIGN = 0x1000
const FILE_ALIGN = 512

func PEWrite(f *os.File, windows_program []byte) {
	pe_offset := int64(512)

	// On which fields are (un)used:
	// http://www.phreedom.org/research/tinype/
	f.Write(header[:])

	idata.Name = ".idata"
	idata.Characteristics = 0xc0000040 // IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE
	idata.Size = 4096

	var bss PESection
	bss.Name = ".bss"
	bss.Characteristics = 0xc0000080 // uninit. data, read, write
	bss.Size = 4096

	var text PESection
	text.Name = ".text"
	text.Characteristics = 0x60000020 // code, read, execute
	text.Size = int64(len(windows_program))

	rsrc.Name = ".rsrc"
	rsrc.Characteristics = 0x40000040 // data, read
	rsrc.Size = 4000

	sections := []*PESection{&idata, &bss, &text, &rsrc}

	headers_sz := pe_offset + 0xf8 + int64(len(sections))*0x28
	{
		section_rva := headers_sz
		section_filepos := headers_sz
		for _, section := range sections {
			section_rva = align_to(section_rva, SEC_ALIGN)
			section.RVA = section_rva
			section_rva += section.Size

			section_filepos = align_to(section_filepos, FILE_ALIGN)
			if section.Characteristics&0x00000070 != 0 {
				section.FilePos = section_filepos
				section_filepos += section.Size
			} else {
				section.FilePos = -1
			}
		}

		f.Seek(align_to(section_filepos, FILE_ALIGN)-1, io.SeekStart)
		write8(f, 0)
	}

	// PE signature.
	f.Seek(pe_offset, io.SeekStart)
	writestrn(f, "PE", 4)

	// IMAGE_FILE_HEADER
	write16(f, 0x014c)                // Machine: IMAGE_FILE_MACHINE_I386
	write16(f, uint16(len(sections))) // NumberOfSections ≤ 96
	write32(f, 0)                     // unused TimeDateStamp
	write32(f, 0)                     // PointerToSymbolTable
	write32(f, 0)                     // NumberOfSymbols
	write16(f, 224)                   // SizeOfOptionalHeader
	write16(f, 0x103)                 // Characteristics = IMAGE_FILE_RELOCS_STRIPPED | IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_32BIT_MACHINE

	// IMAGE_OPTIONAL_HEADER32
	write16(f, 0x10b) // Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC
	write8(f, 0)      // unused MajorLinkerVersion
	write8(f, 0)      // unused MinorLinkerVersion
	{
		var SizeOfCode, SizeOfInitializedData, SizeOfUninitializedData int64
		for _, section := range sections {
			if section.Characteristics&0x00000020 != 0 { // IMAGE_SCN_CNT_CODE
				SizeOfCode += section.Size
			}
			if section.Characteristics&0x00000040 != 0 { // IMAGE_SCN_CNT_INITIALIZED_DATA
				SizeOfInitializedData += section.Size
			}
			if section.Characteristics&0x00000080 != 0 { // IMAGE_SCN_CNT_UNINITIALIZED_DATA
				SizeOfUninitializedData += section.Size
			}
		}
		write32(f, uint32(SizeOfCode))              // ∑ SizeOfCode
		write32(f, uint32(SizeOfInitializedData))   // ∑ SizeOfInitializedData
		write32(f, uint32(SizeOfUninitializedData)) // ∑ SizeOfUninitializedData
	}
	write32(f, uint32(text.RVA)) // AddressOfEntryPoint
	write32(f, uint32(text.RVA)) // BaseOfCode
	write32(f, 0)                // BaseOfData
	write32(f, 0x00400000)       // ImageBase
	write32(f, SEC_ALIGN)        // SectionAlignment
	write32(f, FILE_ALIGN)       // FileAlignment
	write16(f, 3)                // MajorOperatingSystemVersion
	write16(f, 10)               // MinorOperatingSystemVersion
	write16(f, 0)                // unused MajorImageVersion
	write16(f, 0)                // unused MinorImageVersion
	write16(f, 3)                // MajorSubsystemVersion
	write16(f, 10)               // MinorSubsystemVersion
	write32(f, 0)                // unused Win32VersionValue
	write32(f, uint32(           // SizeOfImage
		align_to(sections[len(sections)-1].RVA+sections[len(sections)-1].Size, SEC_ALIGN)))
	write32(f, uint32( // SizeOfHeaders
		align_to(headers_sz, FILE_ALIGN)))
	write32(f, 0)        // Checksum
	write16(f, 3)        // Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI
	write16(f, 0)        // DllCharacteristics
	write32(f, 0x100000) // SizeOfStackReserve
	write32(f, 0x1000)   // SizeOfStackCommit
	write32(f, 0x100000) // SizeOfHeapReserve
	write32(f, 0x1000)   // SizeOfHeapCommit
	write32(f, 0)        // unused LoadFlags
	write32(f, 16)       // NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES

	// IMAGE_DATA_DIRECTORY
	// Sizes are vague. They are probably just useless fields.
	write32(f, 0) // Export Table.
	write32(f, 0)
	write32(f, uint32(idata.RVA)) // Import Table.
	write32(f, uint32(idata.Size))
	write32(f, uint32(rsrc.RVA)) // Resource Table.
	write32(f, uint32(rsrc.Size))
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
	// On useless bound imports and IAT directories:
	// https://stackoverflow.com/a/62850912
	write32(f, 0) // Bound Import.
	write32(f, 0)
	write32(f, 0) // unused IMAGE_DIRECTORY_ENTRY_IAT
	write32(f, 0)
	write32(f, 0) // Delay Import Descriptor.
	write32(f, 0)
	write32(f, 0) // CLR Runtime Header.
	write32(f, 0)
	write32(f, 0) // reserved
	write32(f, 0)

	// IMAGE_SECTION_HEADER
	for _, section := range sections {
		writestrn(f, section.Name, 8)    // Name
		write32(f, uint32(section.Size)) // VirtualSize
		write32(f, uint32(section.RVA))  // VirtualAddress
		if section.FilePos < 0 {
			write32(f, 0) // SizeOfRawData
			write32(f, 0) // PointerToRawData
		} else {
			write32(f, uint32(align_to(section.Size, FILE_ALIGN))) // SizeOfRawData
			write32(f, uint32(section.FilePos))                    // PointerToRawData
		}
		write32(f, 0)                       // PointerToRelocations
		write32(f, 0)                       // PointerToLinenumbers
		write16(f, 0)                       // NumberOfRelocations
		write16(f, 0)                       // NumberOfLinenumbers
		write32(f, section.Characteristics) // Characteristics
	}

	/* Write .idata segment. */
	f.Seek(idata.FilePos, io.SeekStart)
	IDataWrite(f, []IDataDirectoryEntry{
		{LibraryName: "kernel32.dll", Symbols: []string{
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
		}},
	})

	// Write .text segment.
	f.Seek(text.FilePos, io.SeekStart)
	f.Write(windows_program)

	// Write .rsrc segment.
	f.Seek(rsrc.FilePos, io.SeekStart)
	RSRCWriteDirectoryEntry(f, &RSRCDirectoryEntry{IsDirectory: true, Directory: []RSRCDirectoryEntry{
		{ID: 16, IsDirectory: true, Directory: []RSRCDirectoryEntry{
			{ID: 1, IsDirectory: true, Directory: []RSRCDirectoryEntry{
				{ID: 0, IsDirectory: false, Data: RSRCMakeVersionInfoStruct("VS_VERSION_INFO", false,
					RSRCMakeFixedFileInfo(114, 514, 1919, 810),
					[][]byte{
						RSRCMakeVersionInfoStruct("StringFileInfo", false, nil, [][]byte{
							RSRCMakeVersionInfoStruct(fmt.Sprintf("%08X", 0x040904b0), false, nil, [][]byte{
								RSRCMakeVersionInfoStruct("CompanyName", true, LPCWSTR("公司名称"), nil),
								RSRCMakeVersionInfoStruct("FileDescription", true, LPCWSTR("文件描述"), nil),
								RSRCMakeVersionInfoStruct("FileVersion", true, LPCWSTR("文件版本"), nil),
								RSRCMakeVersionInfoStruct("InternalName", true, LPCWSTR("内部名称"), nil),
								RSRCMakeVersionInfoStruct("LegalCopyright", true, LPCWSTR("\u00a9 版权所有"), nil),
								RSRCMakeVersionInfoStruct("OriginalFilename", true, LPCWSTR("slzprog-output.exe"), nil),
								RSRCMakeVersionInfoStruct("ProductName", true, LPCWSTR("产品名称"), nil),
								RSRCMakeVersionInfoStruct("ProductVersion", true, LPCWSTR("产品版本"), nil),
							}),
						}),
						RSRCMakeVersionInfoStruct("VarFileInfo", false, nil, [][]byte{
							RSRCMakeVersionInfoStruct("Translation", false, []byte{0x09, 0x04, 0xb0, 0x04}, nil),
						}),
					},
				)},
			}},
		}},
		{ID: 6, IsDirectory: true, Directory: []RSRCDirectoryEntry{
			{ID: 1, IsDirectory: true, Directory: []RSRCDirectoryEntry{
				// 字符串表中的字符一般不是零结尾的，但是只要一个隔一个放，中间为空字符串计数的零就有用了。
				// Strings stored in string tables are not zero-terminated generally.
				// However, one can interleave useful strings with empty ones.
				// The zero-valued length counters then serve as terminators neatly.
				{ID: 1033, IsDirectory: false, Data: RSRCMakeStringTable([]string{"Grass.", "", "More grass."})},
				{ID: 1041, IsDirectory: false, Data: RSRCMakeStringTable([]string{"くさ。", "", "もっとくさ。"})},
				{ID: 2052, IsDirectory: false, Data: RSRCMakeStringTable([]string{"草。", "", "更草。"})},
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

	f.Seek(rsrc.FilePos+rsrc.Size, io.SeekStart)
	fmt.Fprintf(f, "Data after this point is useless.")
}

func MakeExe(main *Subroutine) {
	somethingfp, _ := os.OpenFile("gen.asm", os.O_RDWR|os.O_CREATE|os.O_TRUNC, 0777)
	outputfp = somethingfp
	emit_noindent("bits 32")
	emit_noindent("org %#x", 0x00403000)
	for i, name := range []string{
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
		emit("%s equ %#x", name, 0x00401028+i*4)
	}
	emit("call main")
	emit("push eax")
	emit("call [ExitProcess]")
	emit_subroutine(main)
	somethingfp.Close()
	if bytes, err := exec.Command("nasm", "gen.asm", "-o", "something.bin").CombinedOutput(); err != nil {
		fmt.Printf("exec.CombinedOutput: %v\n%s", err, bytes)
		os.Exit(1)
	}
	os.Remove("gen.asm")
	f, err := os.Create("slzprog-output.exe")
	if err != nil {
		fmt.Printf("os.Create: %v\n", err)
		os.Exit(1)
	}
	bytes, _ := os.ReadFile("something.bin")
	PEWrite(f, bytes)
	err = f.Close()
	if err != nil {
		fmt.Printf("Close: %v\n", err)
		os.Exit(1)
	}
}

func main() {
}
