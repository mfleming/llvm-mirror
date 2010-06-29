//===- lib/MC/ELFObjectWriter.cpp - ELF File Writer -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements ELF object file writer information.
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/ELFObjectWriter.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/Twine.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCAsmLayout.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/ELF.h"
#include "llvm/Target/TargetAsmBackend.h"

#include <vector>
using namespace llvm;

namespace {

class ELFObjectWriterImpl {

  /// ELFSymbolData - Helper struct for containing some precomputed information
  /// on symbols.
  struct ELFSymbolData {
    MCSymbolData *SymbolData;
    uint64_t StringIndex;
    uint32_t SectionIndex;

    // Support lexicographic sorting.
    bool operator<(const ELFSymbolData &RHS) const {
      const std::string &Name = SymbolData->getSymbol().getName();
      return Name < RHS.SymbolData->getSymbol().getName();
    }
  };

  /// @name Relocation Data
  /// @{

  struct ELFRelocationEntry {
    // Make these big enough for both 32-bit and 64-bit
    uint64_t r_offset;
    uint64_t r_info;
    uint64_t r_addend;
  };

  llvm::DenseMap<const MCSectionData*,
                 std::vector<ELFRelocationEntry> > Relocations;
  DenseMap<const MCSection*, uint64_t> SectionStringTableIndex;

  /// @}
  /// @name Symbol Table Data
  /// @{

  SmallString<256> StringTable;
  std::vector<ELFSymbolData> LocalSymbolData;
  std::vector<ELFSymbolData> ExternalSymbolData;
  std::vector<ELFSymbolData> UndefinedSymbolData;

  /// @}

  ELFObjectWriter *Writer;

  raw_ostream &OS;

  // This holds the current offset into the object file.
  size_t FileOff;

  unsigned Is64Bit : 1;

  bool HasRelocationAddend;

  // This holds the symbol table index of the last local symbol.
  unsigned LastLocalSymbolIndex;
  // This holds the .strtab section index.
  unsigned StringTableIndex;

  unsigned ShstrtabIndex;

public:
  ELFObjectWriterImpl(ELFObjectWriter *_Writer, bool _Is64Bit,
                      bool _HasRelAddend)
    : Writer(_Writer), OS(Writer->getStream()), FileOff(0),
      Is64Bit(_Is64Bit), HasRelocationAddend(_HasRelAddend) {
  }

  void Write8(uint8_t Value) { Writer->Write8(Value); }
  void Write16(uint16_t Value) { Writer->Write16(Value); }
  void Write32(uint32_t Value) { Writer->Write32(Value); }
  void Write64(uint64_t Value) { Writer->Write64(Value); }
  void WriteZeros(unsigned N) { Writer->WriteZeros(N); }
  void WriteBytes(StringRef Str, unsigned ZeroFillSize = 0) {
    Writer->WriteBytes(Str, ZeroFillSize);
  }

  void WriteWord(uint64_t W) {
    if (Is64Bit) {
      Writer->Write64(W);
    } else {
      Writer->Write32(W);
    }
  }

  // Emit the ELF header.
  void WriteHeader(uint64_t SectionDataSize, unsigned NumberOfSections) {

    // ELF Header
    // ----------
    //
    // Note
    // ----
    // emitWord method behaves differently for ELF32 and ELF64, writing
    // 4 bytes in the former and 8 in the latter.

    Write8(0x7f); // e_ident[EI_MAG0]
    Write8('E');  // e_ident[EI_MAG1]
    Write8('L');  // e_ident[EI_MAG2]
    Write8('F');  // e_ident[EI_MAG3]

    Write8(Is64Bit ? ELF::ELFCLASS64 : ELF::ELFCLASS32); // e_ident[EI_CLASS]

    // e_ident[EI_DATA]
    Write8(Writer->isLittleEndian() ? ELF::ELFDATA2LSB : ELF::ELFDATA2MSB);

    Write8(ELF::EV_CURRENT);        // e_ident[EI_VERSION]
    Write8(ELF::ELFOSABI_LINUX);    // e_ident[EI_OSABI]
    Write8(0);			// e_ident[EI_ABIVERSION]

    WriteZeros(ELF::EI_NIDENT - ELF::EI_PAD);

    Write16(ELF::ET_REL);             // e_type

    // FIXME
    Write16(ELF::EM_X86_64); // e_machine = target

    Write32(ELF::EV_CURRENT);         // e_version
    WriteWord(0);                    // e_entry, no entry point in .o file
    WriteWord(0);                    // e_phoff, no program header for .o
    WriteWord(SectionDataSize + 64);  // e_shoff = sec hdr table off in bytes

    // FIXME
    Write32(0);   // e_flags = whatever the target wants

    Write16(Is64Bit ? 64 : 52);  // e_ehsize = ELF header size
    Write16(0);                  // e_phentsize = prog header entry size
    Write16(0);                  // e_phnum = # prog header entries = 0

    // e_shentsize = Section header entry size
    Write16(Is64Bit ? 64 : 40);

    // e_shnum     = # of section header ents
    Write16(NumberOfSections);

    // e_shstrndx  = Section # of '.shstrtab'
    Write16(ShstrtabIndex);
  }

  void WriteSymbolEntry(MCDataFragment *F, uint64_t name, uint8_t info,
                        uint64_t value, uint32_t size,
                        uint8_t other, uint16_t shndx) {
    if (Is64Bit) {
      F->getContents() += StringRef((const char *)&name, 4);  // st_name
      F->getContents() += StringRef((const char *)&info, 1);  // st_info
      F->getContents() += StringRef((const char *)&other, 1); // st_other
      F->getContents() += StringRef((const char *)&shndx, 2); // st_shndx
      F->getContents() += StringRef((const char *)&value, 8); // st_value
      // FIXME
      F->getContents() += StringRef((const char *)&size, 8);  // st_size
    } else {
      F->getContents() += StringRef((const char *)&name, 4);  // st_name
      F->getContents() += StringRef((const char *)&value, 4); // st_value
      // FIXME
      F->getContents() += StringRef((const char *)&size, 4);  // st_size
      F->getContents() += StringRef((const char *)&info, 1);  // st_info
      F->getContents() += StringRef((const char *)&other, 1); // st_other
      F->getContents() += StringRef((const char *)&shndx, 2); // st_shndx
    }
  }

  void WriteSymbol(MCDataFragment *F, ELFSymbolData &MSD,
                   const MCAsmLayout &Layout) {
    MCSymbolData &Data = *MSD.SymbolData;
    const MCSymbol &Symbol = Data.getSymbol();
    uint8_t Info = (Data.getFlags() & 0xff);
    uint8_t Other = ((Data.getFlags() & 0xf00) >> ELF::STV_SHIFT);
    uint64_t Value = 0;
    uint32_t Size = 0;

    // Compute the symbol address.
    if (Symbol.isDefined() && !Symbol.isAbsolute()) {
      Value = Layout.getSymbolAddress(&Data);
      MCValue Res;
      if (Data.getSizeSymbol()->EvaluateAsRelocatable(Res, &Layout)) {
        MCSymbolData &A =
            Layout.getAssembler().getSymbolData(Res.getSymA()->getSymbol());
        MCSymbolData &B =
            Layout.getAssembler().getSymbolData(Res.getSymB()->getSymbol());

	Size = Layout.getSymbolAddress(&A) - Layout.getSymbolAddress(&B);
      }

    } else if (Data.isCommon()) {
      // The symbol value is the alignment of the object.
      Value = Data.getCommonAlignment();
      Size = Data.getCommonSize();
    }

    // Write out the symbol table entry
    WriteSymbolEntry(F, MSD.StringIndex, Info, Value,
                     Size, Other, MSD.SectionIndex);
  }

  void WriteSymbolTable(MCDataFragment *F, const MCAssembler &Asm,
                        const MCAsmLayout &Layout) {

    // The string table must be emitted first because we need the index
    // into the string table for all the symbol names.
    assert(StringTable.size() && "Missing string table");

    // FIXME: Make sure the start of the symbol table is aligned.

    // The first entry is the undefined symbol entry.
    unsigned EntrySize = Is64Bit ? ELF::SYMENTRY_SIZE64 : ELF::SYMENTRY_SIZE32;
    for (unsigned i = 0; i < EntrySize; ++i)
      F->getContents() += '\x00';

    // Write the symbol table entries.
    LastLocalSymbolIndex = LocalSymbolData.size();
    for (unsigned i = 0, e = LocalSymbolData.size(); i != e; ++i) {
      ELFSymbolData &MSD = LocalSymbolData[i];
      WriteSymbol(F, MSD, Layout);
    }

    // Write out a symbol table entry for each section.
    for (unsigned Index = 1; Index < Asm.size(); ++Index)
      WriteSymbolEntry(F, 0, ELF::STT_SECTION, 0, 0, ELF::STV_DEFAULT, Index);
    LastLocalSymbolIndex += Asm.size() - 1;

    for (unsigned i = 0, e = ExternalSymbolData.size(); i != e; ++i) {
      ELFSymbolData &MSD = ExternalSymbolData[i];
      MCSymbolData &Data = *MSD.SymbolData;
      assert((Data.getFlags() & (ELF::STB_GLOBAL << ELF::STB_SHIFT)) &&
	     "External symbol requires STB_GLOBAL flag");
      WriteSymbol(F, MSD, Layout);
    }
    LastLocalSymbolIndex += ExternalSymbolData.size();

    for (unsigned i = 0, e = UndefinedSymbolData.size(); i != e; ++i) {
      ELFSymbolData &MSD = UndefinedSymbolData[i];
      MCSymbolData &Data = *MSD.SymbolData;
      Data.setFlags(Data.getFlags() | (ELF::STB_GLOBAL << ELF::STB_SHIFT));
      WriteSymbol(F, MSD, Layout);
    }
  }

  void RecordRelocation(const MCAssembler &Asm, const MCAsmLayout &Layout,
                        const MCFragment *Fragment, const MCFixup &Fixup,
                        MCValue Target, uint64_t &FixedValue) {
  }

  /// ComputeSymbolTable - Compute the symbol table data
  ///
  /// \param StringTable [out] - The string table data.
  /// \param StringIndexMap [out] - Map from symbol names to offsets in the
  /// string table.
  void ComputeSymbolTable(MCAssembler &Asm) {
    // Build section lookup table.
    DenseMap<const MCSection*, uint8_t> SectionIndexMap;
    unsigned Index = 1;
    for (MCAssembler::iterator it = Asm.begin(),
           ie = Asm.end(); it != ie; ++it, ++Index)
      SectionIndexMap[&it->getSection()] = Index;

    // Index 0 is always the empty string.
    StringMap<uint64_t> StringIndexMap;
    StringTable += '\x00';

    // Add the data for local symbols.
    for (MCAssembler::symbol_iterator it = Asm.symbol_begin(),
           ie = Asm.symbol_end(); it != ie; ++it) {
      const MCSymbol &Symbol = it->getSymbol();

      // Ignore non-linker visible symbols.
      if (!Asm.isSymbolLinkerVisible(Symbol))
        continue;

      if (it->isExternal() || Symbol.isUndefined())
        continue;

      uint64_t &Entry = StringIndexMap[Symbol.getName()];
      if (!Entry) {
        Entry = StringTable.size();
        StringTable += Symbol.getName();
        StringTable += '\x00';
      }

      ELFSymbolData MSD;
      MSD.SymbolData = it;
      MSD.StringIndex = Entry;

      if (Symbol.isAbsolute()) {
        MSD.SectionIndex = ELF::SHN_ABS;
        LocalSymbolData.push_back(MSD);
      } else {
        MSD.SectionIndex = SectionIndexMap.lookup(&Symbol.getSection());
        assert(MSD.SectionIndex && "Invalid section index!");
        LocalSymbolData.push_back(MSD);
      }
    }

    // Now add non-local symbols.
    for (MCAssembler::symbol_iterator it = Asm.symbol_begin(),
           ie = Asm.symbol_end(); it != ie; ++it) {
      const MCSymbol &Symbol = it->getSymbol();

      // Ignore non-linker visible symbols.
      if (!Asm.isSymbolLinkerVisible(Symbol))
        continue;

      if (!it->isExternal() && !Symbol.isUndefined())
        continue;

      uint64_t &Entry = StringIndexMap[Symbol.getName()];
      if (!Entry) {
        Entry = StringTable.size();
        StringTable += Symbol.getName();
        StringTable += '\x00';
      }

      ELFSymbolData MSD;
      MSD.SymbolData = it;
      MSD.StringIndex = Entry;

      if (Symbol.isUndefined()) {
        MSD.SectionIndex = ELF::SHN_UNDEF;
	// XXX: for some reason we dont Emit* this
	it->setFlags(it->getFlags() | (ELF::STB_GLOBAL << ELF::STB_SHIFT));
        UndefinedSymbolData.push_back(MSD);
      } else if (Symbol.isAbsolute()) {
        MSD.SectionIndex = ELF::SHN_ABS;
        ExternalSymbolData.push_back(MSD);
      } else if (it->isCommon()) {
        MSD.SectionIndex = ELF::SHN_COMMON;
        ExternalSymbolData.push_back(MSD);
      } else {
        MSD.SectionIndex = SectionIndexMap.lookup(&Symbol.getSection());
        assert(MSD.SectionIndex && "Invalid section index!");
        ExternalSymbolData.push_back(MSD);
      }
    }

    // Symbols are required to be in lexicographic order.
    array_pod_sort(LocalSymbolData.begin(), LocalSymbolData.end());
    array_pod_sort(ExternalSymbolData.begin(), ExternalSymbolData.end());
    array_pod_sort(UndefinedSymbolData.begin(), UndefinedSymbolData.end());

    // Set the symbol indices. Local symbols must come before all other
    // symbols with non-local bindings.
    Index = 0;
    for (unsigned i = 0, e = LocalSymbolData.size(); i != e; ++i)
      LocalSymbolData[i].SymbolData->setIndex(Index++);
    for (unsigned i = 0, e = ExternalSymbolData.size(); i != e; ++i)
      ExternalSymbolData[i].SymbolData->setIndex(Index++);
    for (unsigned i = 0, e = UndefinedSymbolData.size(); i != e; ++i)
      UndefinedSymbolData[i].SymbolData->setIndex(Index++);
  }

  void CreateMetadataSections(MCAssembler &Asm, MCAsmLayout &Layout) {
    MCContext &Ctx = Asm.getContext();

    const MCSection *SymtabSection;
    SymtabSection = Ctx.getELFSection(".symtab", ELF::SHT_SYMTAB, 0,
                                      SectionKind::getReadOnly(), false);

    MCSectionData &SymtabSD = Asm.getOrCreateSectionData(*SymtabSection);

    SymtabSD.setAlignment(Is64Bit ? 8 : 4);
    SymtabSD.setEntrySize(Is64Bit ? ELF::SYMENTRY_SIZE64 : ELF::SYMENTRY_SIZE32);

    MCDataFragment *F = new MCDataFragment(&SymtabSD);

    // Symbol table
    WriteSymbolTable(F, Asm, Layout);
    Asm.AddSectionToTheEnd(SymtabSD, Layout);

    const MCSection *StrtabSection;
    StrtabSection = Ctx.getELFSection(".strtab", ELF::SHT_STRTAB, 0,
				      SectionKind::getReadOnly(), false);

    MCSectionData &StrtabSD = Asm.getOrCreateSectionData(*StrtabSection);
    StrtabSD.setAlignment(1);

    // FIXME: This isn't right. If the sections get rearranged this will
    // be wrong. We need a proper lookup.
    StringTableIndex = Asm.size();

    F = new MCDataFragment(&StrtabSD);
    F->getContents().append(StringTable.begin(), StringTable.end());
    Asm.AddSectionToTheEnd(StrtabSD, Layout);

    const MCSection *ShstrtabSection;
    ShstrtabSection = Ctx.getELFSection(".shstrtab", ELF::SHT_STRTAB, 0,
					SectionKind::getReadOnly(), false);

    MCSectionData &ShstrtabSD = Asm.getOrCreateSectionData(*ShstrtabSection);
    ShstrtabSD.setAlignment(1);

    F = new MCDataFragment(&ShstrtabSD);

    // FIXME: This isn't right. If the sections get rearranged this will
    // be wrong. We need a proper lookup.
    ShstrtabIndex = Asm.size();

    // Section header string table.
    //
    // The first entry of a string table holds a null character so skip
    // section 0.
    uint64_t Index = 1;
    F->getContents() += '\x00';

    for (MCAssembler::const_iterator it = Asm.begin(),
           ie = Asm.end(); it != ie; ++it) {
      const MCSectionData &SD = *it;
      const MCSectionELF &Section =
        static_cast<const MCSectionELF&>(SD.getSection());


      // Remember the index into the string table so we can write it
      // into the sh_name field of the section header table.
      SectionStringTableIndex[&it->getSection()] = Index;

      Index += Section.getSectionName().size() + 1;
      F->getContents() += Section.getSectionName();
      F->getContents() += '\x00';
    }

    Asm.AddSectionToTheEnd(ShstrtabSD, Layout);
  }

  void ExecutePostLayoutBinding(MCAssembler &Asm) {
  }

  void WriteSecHdrEntry(uint32_t Name, uint32_t Type, uint64_t Flags,
			uint64_t Address, uint64_t Offset,
			uint64_t Size, uint32_t Link, uint32_t Info,
			uint64_t Alignment, uint64_t EntrySize) {
    Write32(Name);        // sh_name: index into string table
    Write32(Type);        // sh_type
    WriteWord(Flags);     // sh_flags
    WriteWord(Address);   // sh_addr
    WriteWord(Offset);    // sh_offset
    WriteWord(Size);      // sh_size
    Write32(Link);        // sh_link
    Write32(Info);        // sh_info
    WriteWord(Alignment); // sh_addralign
    WriteWord(EntrySize); // sh_entsize
  }

  void WriteObject(const MCAssembler &Asm, const MCAsmLayout &Layout) {
    // Compute symbol table information.
    ComputeSymbolTable(const_cast<MCAssembler&>(Asm));

    CreateMetadataSections(const_cast<MCAssembler&>(Asm),
                           const_cast<MCAsmLayout&>(Layout));

    // Add 1 for the null section.
    unsigned NumSections = Asm.size() + 1;

    uint64_t SectionDataSize = 0;

    for (MCAssembler::const_iterator it = Asm.begin(),
           ie = Asm.end(); it != ie; ++it) {
      const MCSectionData &SD = *it;

      // Get the size of the section in the output file (including padding).
      uint64_t Size = Layout.getSectionFileSize(&SD);
      SectionDataSize += Size;
    }

    // Write out the ELF header ...
    WriteHeader(SectionDataSize, NumSections);
    FileOff = Is64Bit ? 64 : 52;

    // ... then all of the sections ...
    DenseMap<const MCSection*, uint64_t> SectionOffsetMap;

    for (MCAssembler::const_iterator it = Asm.begin(),
           ie = Asm.end(); it != ie; ++it) {
      // Remember the offset into the file for this section.
      SectionOffsetMap[&it->getSection()] = FileOff;

      const MCSectionData &SD = *it;
      FileOff += Layout.getSectionFileSize(&SD);

      Asm.WriteSectionData(it, Layout, Writer);
    }

    // Relocation section
    llvm::DenseMap<const MCSectionData*,
                 std::vector<ELFRelocationEntry> > Relocations;
    for (MCAssembler::const_iterator it = Asm.begin(),
           ie = Asm.end(); it != ie; ++it) {
      std::vector<ELFRelocationEntry> &Relocs = Relocations[it];

      for (unsigned i = 0, e = Relocs.size(); i != e; ++i) {
        ELFRelocationEntry entry = Relocs[e - i - 1];

        if (Is64Bit) {
          Write64(entry.r_offset);   // r_offset
          Write64(entry.r_info);     // r_info

          if (HasRelocationAddend)
            Write64(entry.r_addend); // r_addend
        } else {
          Write32(entry.r_offset);   // r_offset
          Write32(entry.r_info);     // r_info

	  if (HasRelocationAddend)
            Write32(entry.r_addend); // r_addend
        }
      }
    }

    // ... and then the section header table.
    // Should we align the section header table?
    //
    // Null secton first.
    WriteSecHdrEntry(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    for (MCAssembler::const_iterator it = Asm.begin(),
           ie = Asm.end(); it != ie; ++it) {
      const MCSectionData &SD = *it;
      const MCSectionELF &Section =
        static_cast<const MCSectionELF&>(SD.getSection());
      std::vector<ELFRelocationEntry> &Relocs = Relocations[it];;

      uint64_t sh_link = 0;
      uint64_t sh_info = 0;

      switch(Section.getType()) {
      case ELF::SHT_DYNAMIC:
        sh_link = SectionStringTableIndex[&it->getSection()];
        sh_info = 0;
        break;

      case ELF::SHT_REL:
      case ELF::SHT_RELA:
        // Make sure we have some relocs.
        assert(Relocs.size() && "SHT_REL[A] set but couldn't find relocs");
        sh_link = 0;
        sh_info = 0;
        break;

      case ELF::SHT_SYMTAB:
      case ELF::SHT_DYNSYM:
        sh_link = StringTableIndex;
        sh_info = LastLocalSymbolIndex;
        break;

      case ELF::SHT_HASH:
      case ELF::SHT_GROUP:
      case ELF::SHT_SYMTAB_SHNDX:
        assert(0 && "FIXME: sh_type value not supported!");
        break;
      }

      WriteSecHdrEntry(SectionStringTableIndex[&it->getSection()],
		       Section.getType(), Section.getFlags(),
		       Layout.getSectionAddress(&SD),
		       SectionOffsetMap.lookup(&SD.getSection()),
		       Layout.getSectionSize(&SD), sh_link,
                       sh_info, SD.getAlignment(),
                       SD.getEntrySize());
    }
  }
};

}

ELFObjectWriter::ELFObjectWriter(raw_ostream &OS,
                                 bool Is64Bit,
                                 bool IsLittleEndian,
                                 bool HasRelocationAddend)
  : MCObjectWriter(OS, IsLittleEndian)
{
  Impl = new ELFObjectWriterImpl(this, Is64Bit, HasRelocationAddend);
}

ELFObjectWriter::~ELFObjectWriter() {
  delete (ELFObjectWriterImpl*) Impl;
}

void ELFObjectWriter::ExecutePreLayoutBinding(MCAssembler &Asm, MCAsmLayout &Layout) {
  ((ELFObjectWriterImpl*) Impl)->ExecutePreLayoutBinding(Asm, Layout);
}

void ELFObjectWriter::ExecutePostLayoutBinding(MCAssembler &Asm) {
  ((ELFObjectWriterImpl*) Impl)->ExecutePostLayoutBinding(Asm);
}

void ELFObjectWriter::RecordRelocation(const MCAssembler &Asm,
                                        const MCAsmLayout &Layout,
                                        const MCFragment *Fragment,
                                        const MCFixup &Fixup, MCValue Target,
                                        uint64_t &FixedValue) {
  ((ELFObjectWriterImpl*) Impl)->RecordRelocation(Asm, Layout, Fragment, Fixup,
                                                   Target, FixedValue);
}

void ELFObjectWriter::WriteObject(const MCAssembler &Asm,
                                   const MCAsmLayout &Layout) {
  ((ELFObjectWriterImpl*) Impl)->WriteObject(Asm, Layout);
}
