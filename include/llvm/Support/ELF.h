//===-- llvm/Support/ELF.h - ELF constants and data structures --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This header contains common, non-processor-specific data structures and
// constants for the ELF file format.
//
// The details of the ELF32 bits in this file are largely based on
// the Tool Interface Standard (TIS) Executable and Linking Format
// (ELF) Specification Version 1.2, May 1995. The ELF64 stuff is not
// standardized, as far as I can tell. It was largely based on information
// I found in OpenBSD header files.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_ELF_H
#define LLVM_SUPPORT_ELF_H

#include "llvm/System/DataTypes.h"
#include <cstring>

namespace llvm {

namespace ELF {

typedef uint32_t Elf32_Addr; // Program address
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;  // File offset
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Word;

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef int32_t  Elf64_Shalf;
typedef int32_t  Elf64_Sword;
typedef uint32_t Elf64_Word;
typedef int64_t  Elf64_Sxword;
typedef uint64_t Elf64_Xword;
typedef uint32_t Elf64_Half;
typedef uint16_t Elf64_Quarter;

// Object file magic string.
static const char ElfMagic[] = { 0x7f, 'E', 'L', 'F', '\0' };

struct Elf32_Ehdr {
  unsigned char e_ident[16]; // ELF Identification bytes
  Elf32_Half    e_type;      // Type of file (see ET_* below)
  Elf32_Half    e_machine;   // Required architecture for this file (see EM_*)
  Elf32_Word    e_version;   // Must be equal to 1
  Elf32_Addr    e_entry;     // Address to jump to in order to start program
  Elf32_Off     e_phoff;     // Program header table's file offset, in bytes
  Elf32_Off     e_shoff;     // Section header table's file offset, in bytes
  Elf32_Word    e_flags;     // Processor-specific flags
  Elf32_Half    e_ehsize;    // Size of ELF header, in bytes
  Elf32_Half    e_phentsize; // Size of an entry in the program header table
  Elf32_Half    e_phnum;     // Number of entries in the program header table
  Elf32_Half    e_shentsize; // Size of an entry in the section header table
  Elf32_Half    e_shnum;     // Number of entries in the section header table
  Elf32_Half    e_shstrndx;  // Sect hdr table index of sect name string table
  bool checkMagic () const {
    return (memcmp (e_ident, ElfMagic, strlen (ElfMagic))) == 0;
  }
  unsigned char getFileClass () const { return e_ident[4]; }
  unsigned char getDataEncoding () { return e_ident[5]; }
};

// 64-bit ELF header. Fields are the same as for ELF32, but with different
// types (see above).
struct Elf64_Ehdr {
  unsigned char e_ident[16];
  Elf64_Quarter e_type;
  Elf64_Quarter e_machine;
  Elf64_Half    e_version;
  Elf64_Addr    e_entry;
  Elf64_Off     e_phoff;
  Elf64_Off     e_shoff;
  Elf64_Half    e_flags;
  Elf64_Quarter e_ehsize;
  Elf64_Quarter e_phentsize;
  Elf64_Quarter e_phnum;
  Elf64_Quarter e_shentsize;
  Elf64_Quarter e_shnum;
  Elf64_Quarter e_shstrndx;
};

// ELF header e_ident offsets
enum {
  EI_MAG0       = 0, // File identification
  EI_MAG1       = 1, // File identification
  EI_MAG2       = 2, // File identification
  EI_MAG3       = 3, // File identification
  EI_CLASS      = 4, // File class
  EI_DATA       = 5, // Data encoding
  EI_VERSION    = 6, // File version
  EI_OSABI      = 7, // Operating system/ABI identification
  EI_ABIVERSION = 8, // ABI version
  EI_PAD        = 9, // Starting of padding bytes
  EI_NIDENT     = 16 // Size of e_ident[]
};

// File types
enum {
  ET_NONE   = 0,      // No file type
  ET_REL    = 1,      // Relocatable file
  ET_EXEC   = 2,      // Executable file
  ET_DYN    = 3,      // Shared object file
  ET_CORE   = 4,      // Core file
  ET_LOPROC = 0xff00, // Beginning of processor-specific codes
  ET_HIPROC = 0xffff  // Processor-specific
};

// Versioning
enum {
  EV_NONE = 0,
  EV_CURRENT = 1
};

// Machine architectures
enum {
  EM_NONE = 0,  // No machine
  EM_M32 = 1,   // AT&T WE 32100
  EM_SPARC = 2, // SPARC
  EM_386 = 3,   // Intel 386
  EM_68K = 4,   // Motorola 68000
  EM_88K = 5,   // Motorola 88000
  EM_486 = 6,   // Intel 486 (deprecated)
  EM_860 = 7,   // Intel 80860
  EM_MIPS = 8,     // MIPS R3000
  EM_PPC = 20,     // PowerPC
  EM_ARM = 40,     // ARM
  EM_ALPHA = 41,   // DEC Alpha
  EM_SPARCV9 = 43, // SPARC V9
  EM_X86_64 = 62   // AMD64
};

// Object file classes.
enum {
  ELFCLASS32 = 1, // 32-bit object file
  ELFCLASS64 = 2  // 64-bit object file
};

// Object file byte orderings.
enum {
  ELFDATA2LSB = 1, // Little-endian object file
  ELFDATA2MSB = 2  // Big-endian object file
};

// OS ABI identification.
enum {
  ELFOSABI_NONE = 0,          // UNIX System V ABI
  ELFOSABI_HPUX = 1,          // HP-UX operating system
  ELFOSABI_NETBSD = 2,        // NetBSD
  ELFOSABI_LINUX = 3,         // GNU/Linux
  ELFOSABI_HURD = 4,          // GNU/Hurd
  ELFOSABI_SOLARIS = 6,       // Solaris
  ELFOSABI_AIX = 7,           // AIX
  ELFOSABI_IRIX = 8,          // IRIX
  ELFOSABI_FREEBSD = 9,       // FreeBSD
  ELFOSABI_TRU64 = 10,        // TRU64 UNIX
  ELFOSABI_MODESTO = 11,      // Novell Modesto
  ELFOSABI_OPENBSD = 12,      // OpenBSD
  ELFOSABI_OPENVMS = 13,      // OpenVMS
  ELFOSABI_NSK = 14,          // Hewlett-Packard Non-Stop Kernel
  ELFOSABI_AROS = 15,         // AROS
  ELFOSABI_FENIXOS = 16,      // FenixOS
  ELFOSABI_C6000_ELFABI = 64, // Bare-metal TMS320C6000
  ELFOSABI_C6000_LINUX = 65,  // Linux TMS320C6000
  ELFOSABI_ARM = 97,          // ARM
  ELFOSABI_STANDALONE = 255   // Standalone (embedded) application
};

// X86_64 relocations
enum {
  R_X86_64_NONE       = 0,
  R_X86_64_64         = 1,
  R_X86_64_PC32       = 2,
  R_X86_64_GOT32      = 3,
  R_X86_64_PLT32      = 4,
  R_X86_64_COPY       = 5,
  R_X86_64_GLOB_DAT   = 6,
  R_X86_64_JUMP_SLOT  = 7,
  R_X86_64_RELATIVE   = 8,
  R_X86_64_GOTPCREL   = 9,
  R_X86_64_32         = 10,
  R_X86_64_32S        = 11,
  R_X86_64_16         = 12,
  R_X86_64_PC16       = 13,
  R_X86_64_8          = 14,
  R_X86_64_PC8        = 15,
  R_X86_64_DTPMOD64   = 16,
  R_X86_64_DTPOFF64   = 17,
  R_X86_64_TPOFF64    = 18,
  R_X86_64_TLSGD      = 19,
  R_X86_64_TLSLD      = 20,
  R_X86_64_DTPOFF32   = 21,
  R_X86_64_GOTTPOFF   = 22,
  R_X86_64_TPOFF32    = 23,
  R_X86_64_PC64       = 24,
  R_X86_64_GOTOFF64   = 25,
  R_X86_64_GOTPC32    = 26,
  R_X86_64_SIZE32     = 32,
  R_X86_64_SIZE64     = 33,
  R_X86_64_GOTPC32_TLSDESC = 34,
  R_X86_64_TLSDESC_CALL    = 35,
  R_X86_64_TLSDESC    = 36
};

// Section header.
struct Elf32_Shdr {
  Elf32_Word sh_name;      // Section name (index into string table)
  Elf32_Word sh_type;      // Section type (SHT_*)
  Elf32_Word sh_flags;     // Section flags (SHF_*)
  Elf32_Addr sh_addr;      // Address where section is to be loaded
  Elf32_Off  sh_offset;    // File offset of section data, in bytes
  Elf32_Word sh_size;      // Size of section, in bytes
  Elf32_Word sh_link;      // Section type-specific header table index link
  Elf32_Word sh_info;      // Section type-specific extra information
  Elf32_Word sh_addralign; // Section address alignment
  Elf32_Word sh_entsize;   // Size of records contained within the section
};

// Section header for ELF64 - same fields as ELF32, different types.
struct Elf64_Shdr {
  Elf64_Half  sh_name;
  Elf64_Half  sh_type;
  Elf64_Xword sh_flags;
  Elf64_Addr  sh_addr;
  Elf64_Off   sh_offset;
  Elf64_Xword sh_size;
  Elf64_Half  sh_link;
  Elf64_Half  sh_info;
  Elf64_Xword sh_addralign;
  Elf64_Xword sh_entsize;
};

// Special section indices.
enum {
  SHN_UNDEF     = 0,      // Undefined, missing, irrelevant, or meaningless
  SHN_LORESERVE = 0xff00, // Lowest reserved index
  SHN_LOPROC    = 0xff00, // Lowest processor-specific index
  SHN_HIPROC    = 0xff1f, // Highest processor-specific index
  SHN_ABS       = 0xfff1, // Symbol has absolute value; does not need relocation
  SHN_COMMON    = 0xfff2, // FORTRAN COMMON or C external global variables
  SHN_HIRESERVE = 0xffff  // Highest reserved index
};

// Section types.
enum {
  SHT_NULL     = 0,  // No associated section (inactive entry).
  SHT_PROGBITS = 1,  // Program-defined contents.
  SHT_SYMTAB   = 2,  // Symbol table.
  SHT_STRTAB   = 3,  // String table.
  SHT_RELA     = 4,  // Relocation entries; explicit addends.
  SHT_HASH     = 5,  // Symbol hash table.
  SHT_DYNAMIC  = 6,  // Information for dynamic linking.
  SHT_NOTE     = 7,  // Information about the file.
  SHT_NOBITS   = 8,  // Data occupies no space in the file.
  SHT_REL      = 9,  // Relocation entries; no explicit addends.
  SHT_SHLIB    = 10, // Reserved.
  SHT_DYNSYM   = 11, // Symbol table.
  SHT_GROUP    = 17, // Section contains a section group.
  SHT_SYMTAB_SHNDX = 18, // Indicies for SHN_XINDEX entries.
  SHT_LOPROC   = 0x70000000, // Lowest processor architecture-specific type.
  SHT_HIPROC   = 0x7fffffff, // Highest processor architecture-specific type.
  SHT_LOUSER   = 0x80000000, // Lowest type reserved for applications.
  SHT_HIUSER   = 0xffffffff  // Highest type reserved for applications.
};

// Section flags.
enum {
  SHF_WRITE     = 0x1, // Section data should be writable during execution.
  SHF_ALLOC     = 0x2, // Section occupies memory during program execution.
  SHF_EXECINSTR = 0x4, // Section contains executable machine instructions.
  SHF_MASKPROC  = 0xf0000000 // Bits indicating processor-specific flags.
};

// Symbol table entries.
struct Elf32_Sym {
  Elf32_Word    st_name;  // Symbol name (index into string table)
  Elf32_Addr    st_value; // Value or address associated with the symbol
  Elf32_Word    st_size;  // Size of the symbol
  unsigned char st_info;  // Symbol's type and binding attributes
  unsigned char st_other; // Must be zero; reserved
  Elf32_Half    st_shndx; // Which section (header table index) it's defined in

  // These accessors and mutators correspond to the ELF32_ST_BIND,
  // ELF32_ST_TYPE, and ELF32_ST_INFO macros defined in the ELF specification:
  unsigned char getBinding () const { return st_info >> 4; }
  unsigned char getType () const { return st_info & 0x0f; }
  void setBinding (unsigned char b) { setBindingAndType (b, getType ()); }
  void setType (unsigned char t) { setBindingAndType (getBinding (), t); }
  void setBindingAndType (unsigned char b, unsigned char t) {
    st_info = (b << 4) + (t & 0x0f);
  }
};

// The size (in bytes) of symbol table entries.
enum {
  SYMENTRY_SIZE32 = 16, // 32-bit symbol entry size
  SYMENTRY_SIZE64 = 24  // 64-bit symbol entry size.
};

// Symbol bindings.
enum {
  STB_LOCAL = 0,   // Local symbol, not visible outside obj file containing def
  STB_GLOBAL = 1,  // Global symbol, visible to all object files being combined
  STB_WEAK = 2,    // Weak symbol, like global but lower-precedence
  STB_LOPROC = 13, // Lowest processor-specific binding type
  STB_HIPROC = 15  // Highest processor-specific binding type
};

// Symbol types.
enum {
  STT_NOTYPE  = 0,   // Symbol's type is not specified
  STT_OBJECT  = 1,   // Symbol is a data object (variable, array, etc.)
  STT_FUNC    = 2,   // Symbol is executable code (function, etc.)
  STT_SECTION = 3,   // Symbol refers to a section
  STT_FILE    = 4,   // Local, absolute symbol that refers to a file
  STT_COMMON  = 5,   // An uninitialised common block
  STT_TLS     = 6,   // Thread local data object
  STT_LOPROC  = 13,  // Lowest processor-specific symbol type
  STT_HIPROC  = 15   // Highest processor-specific symbol type
};

enum {
  STV_DEFAULT   = 0,  // Symbol's visibility is as spcecfied by the binding type
  STV_INTERNAL  = 1,  // OS-specific version of STV_HIDDEN
  STV_HIDDEN    = 2,  // Symbol is only visible inside the current component
  STV_PROTECTED = 3   // Treat as STB_LOCAL inside current component
};

// Because all the symbol flags need to be stored in the MCSymbolData
// 'flags' variable we need to provide shift constants per flag type.
enum {
  STT_SHIFT = 0, // Shift value for STT_* flags.
  STB_SHIFT = 4, // Shift value for STB_* flags.
  STV_SHIFT = 8  // Shift value ofr STV_* flags.
};

// Relocation entry, without explicit addend.
struct Elf32_Rel {
  Elf32_Addr r_offset; // Location (file byte offset, or program virtual addr)
  Elf32_Word r_info;   // Symbol table index and type of relocation to apply

  // These accessors and mutators correspond to the ELF32_R_SYM, ELF32_R_TYPE,
  // and ELF32_R_INFO macros defined in the ELF specification:
  Elf32_Word getSymbol () const { return (r_info >> 8); }
  unsigned char getType () const { return (unsigned char) (r_info & 0x0ff); }
  void setSymbol (Elf32_Word s) { setSymbolAndType (s, getType ()); }
  void setType (unsigned char t) { setSymbolAndType (getSymbol(), t); }
  void setSymbolAndType (Elf32_Word s, unsigned char t) {
    r_info = (s << 8) + t;
  };
};

// Relocation entry with explicit addend.
struct Elf32_Rela {
  Elf32_Addr  r_offset; // Location (file byte offset, or program virtual addr)
  Elf32_Word  r_info;   // Symbol table index and type of relocation to apply
  Elf32_Sword r_addend; // Compute value for relocatable field by adding this

  // These accessors and mutators correspond to the ELF32_R_SYM, ELF32_R_TYPE,
  // and ELF32_R_INFO macros defined in the ELF specification:
  Elf32_Word getSymbol () const { return (r_info >> 8); }
  unsigned char getType () const { return (unsigned char) (r_info & 0x0ff); }
  void setSymbol (Elf32_Word s) { setSymbolAndType (s, getType ()); }
  void setType (unsigned char t) { setSymbolAndType (getSymbol(), t); }
  void setSymbolAndType (Elf32_Word s, unsigned char t) {
    r_info = (s << 8) + t;
  };
};

// Relocation entry, without explicit addend.
struct Elf64_Rel {
  Elf64_Addr r_offset; // Location (file byte offset, or program virtual addr)
  Elf64_Xword r_info;   // Symbol table index and type of relocation to apply

  // These accessors and mutators correspond to the ELF64_R_SYM, ELF64_R_TYPE,
  // and ELF64_R_INFO macros defined in the ELF specification:
  Elf64_Xword getSymbol () const { return (r_info >> 32); }
  unsigned char getType () const { return (unsigned char) (r_info & 0xffffffffL); }
  void setSymbol (Elf32_Word s) { setSymbolAndType (s, getType ()); }
  void setType (unsigned char t) { setSymbolAndType (getSymbol(), t); }
  void setSymbolAndType (Elf64_Xword s, unsigned char t) {
    r_info = (s << 32) + (t&0xffffffffL);
  };
};

// Relocation entry with explicit addend.
struct Elf64_Rela {
  Elf64_Addr  r_offset; // Location (file byte offset, or program virtual addr)
  Elf64_Xword  r_info;   // Symbol table index and type of relocation to apply
  Elf64_Sxword r_addend; // Compute value for relocatable field by adding this

  // These accessors and mutators correspond to the ELF64_R_SYM, ELF64_R_TYPE,
  // and ELF64_R_INFO macros defined in the ELF specification:
  Elf64_Xword getSymbol () const { return (r_info >> 32); }
  unsigned char getType () const { return (unsigned char) (r_info & 0xffffffffL); }
  void setSymbol (Elf64_Xword s) { setSymbolAndType (s, getType ()); }
  void setType (unsigned char t) { setSymbolAndType (getSymbol(), t); }
  void setSymbolAndType (Elf64_Xword s, unsigned char t) {
    r_info = (s << 32) + (t&0xffffffffL);
  };
};

// Program header.
struct Elf32_Phdr {
  Elf32_Word p_type;   // Type of segment
  Elf32_Off  p_offset; // File offset where segment is located, in bytes
  Elf32_Addr p_vaddr;  // Virtual address of beginning of segment
  Elf32_Addr p_paddr;  // Physical address of beginning of segment (OS-specific)
  Elf32_Word p_filesz; // Num. of bytes in file image of segment (may be zero)
  Elf32_Word p_memsz;  // Num. of bytes in mem image of segment (may be zero)
  Elf32_Word p_flags;  // Segment flags
  Elf32_Word p_align;  // Segment alignment constraint
};

// Segment types.
enum {
  PT_NULL    = 0, // Unused segment.
  PT_LOAD    = 1, // Loadable segment.
  PT_DYNAMIC = 2, // Dynamic linking information.
  PT_INTERP  = 3, // Interpreter pathname.
  PT_NOTE    = 4, // Auxiliary information.
  PT_SHLIB   = 5, // Reserved.
  PT_PHDR    = 6, // The program header table itself.
  PT_LOPROC  = 0x70000000, // Lowest processor-specific program hdr entry type.
  PT_HIPROC  = 0x7fffffff  // Highest processor-specific program hdr entry type.
};

// Segment flag bits.
enum {
  PF_X        = 1,         // Execute
  PF_W        = 2,         // Write
  PF_R        = 4,         // Read
  PF_MASKPROC = 0xf0000000 // Unspecified
};

} // end namespace ELF

} // end namespace llvm

#endif
