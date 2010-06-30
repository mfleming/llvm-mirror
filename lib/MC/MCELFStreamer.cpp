//===- lib/MC/MCELFStreamer.cpp - ELF Object Output ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file assembles .s files and emits ELF .o object files.
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCStreamer.h"

#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ELF.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetAsmBackend.h"

using namespace llvm;

namespace {

class MCELFStreamer : public MCStreamer {
private:
  MCAssembler Assembler;
  MCSectionData *CurSectionData;

private:
  MCFragment *getCurrentFragment() const {
    assert(CurSectionData && "No current section!");

    if (!CurSectionData->empty())
      return &CurSectionData->getFragmentList().back();

    return 0;
  }

  /// Get a data fragment to write into, creating a new one if the current
  /// fragment is not a data fragment.
  MCDataFragment *getOrCreateDataFragment() const {
    MCDataFragment *F = dyn_cast_or_null<MCDataFragment>(getCurrentFragment());
    if (!F)
      F = new MCDataFragment(CurSectionData);
    return F;
  }

public:
  MCELFStreamer(MCContext &Context, TargetAsmBackend &TAB,
                  raw_ostream &_OS, MCCodeEmitter *_Emitter)
    : MCStreamer(Context), Assembler(Context, TAB, *_Emitter, _OS),
      CurSectionData(0) {}
  ~MCELFStreamer() {}

  MCAssembler &getAssembler() { return Assembler; }

  const MCExpr *AddValueSymbols(const MCExpr *Value) {
    switch (Value->getKind()) {
    case MCExpr::Target: assert(0 && "Can't handle target exprs yet!");
    case MCExpr::Constant:
      break;

    case MCExpr::Binary: {
      const MCBinaryExpr *BE = cast<MCBinaryExpr>(Value);
      AddValueSymbols(BE->getLHS());
      AddValueSymbols(BE->getRHS());
      break;
    }

    case MCExpr::SymbolRef:
      Assembler.getOrCreateSymbolData(
        cast<MCSymbolRefExpr>(Value)->getSymbol());
      break;

    case MCExpr::Unary:
      AddValueSymbols(cast<MCUnaryExpr>(Value)->getSubExpr());
      break;
    }

    return Value;
  }

  /// @name MCStreamer Interface
  /// @{

  virtual void SwitchSection(const MCSection *Section);
  virtual void EmitLabel(MCSymbol *Symbol);
  virtual void EmitAssemblerFlag(MCAssemblerFlag Flag);
  virtual void EmitAssignment(MCSymbol *Symbol, const MCExpr *Value);
  virtual void EmitSymbolAttribute(MCSymbol *Symbol, MCSymbolAttr Attribute);
  virtual void EmitSymbolDesc(MCSymbol *Symbol, unsigned DescValue);
  virtual void EmitCommonSymbol(MCSymbol *Symbol, uint64_t Size,
                                unsigned ByteAlignment);
  virtual void BeginCOFFSymbolDef(const MCSymbol *Symbol) {
    assert(0 && "ELF doesn't support this directive");
  }

  virtual void EmitCOFFSymbolStorageClass(int StorageClass) {
    assert(0 && "ELF doesn't support this directive");
  }

  virtual void EmitCOFFSymbolType(int Type) {
    assert(0 && "ELF doesn't support this directive");
  }

  virtual void EndCOFFSymbolDef() {
    assert(0 && "ELF doesn't support this directive");
  }

  virtual void EmitELFSize(MCSymbol *Symbol, const MCExpr *Value) {
     MCSymbolData &SD = Assembler.getOrCreateSymbolData(*Symbol);
     uint64_t Size = 0;
     const MCBinaryExpr *BE = NULL;

     if (Value->getKind() == MCExpr::Constant) {
       const MCConstantExpr *CE;
       CE = static_cast<const MCConstantExpr *>(Value);
       Size = CE->getValue();
     } else if (Value->getKind() == MCExpr::Binary) {
       BE = static_cast<const MCBinaryExpr *>(Value);
     }

     if (Size)
       SD.setCommon(Size, Size);
     if (BE)
       SD.setSizeSymbol(BE);
  }

  virtual void EmitLocalCommonSymbol(MCSymbol *Symbol, uint64_t Size) {
    assert(0 && "ELF doesn't support this directive");
  }
  virtual void EmitZerofill(const MCSection *Section, MCSymbol *Symbol = 0,
                            unsigned Size = 0, unsigned ByteAlignment = 0);
  virtual void EmitTBSSSymbol(const MCSection *Section, MCSymbol *Symbol,
			      uint64_t Size, unsigned ByteAlignment = 0);
  virtual void EmitBytes(StringRef Data, unsigned AddrSpace);
  virtual void EmitValue(const MCExpr *Value, unsigned Size,unsigned AddrSpace);
  virtual void EmitGPRel32Value(const MCExpr *Value) {
    assert(0 && "ELF doesn't support this directive");
  }
  virtual void EmitValueToAlignment(unsigned ByteAlignment, int64_t Value = 0,
                                    unsigned ValueSize = 1,
                                    unsigned MaxBytesToEmit = 0);
  virtual void EmitCodeAlignment(unsigned ByteAlignment,
                                 unsigned MaxBytesToEmit = 0);
  virtual void EmitValueToOffset(const MCExpr *Offset,
                                 unsigned char Value = 0);

  virtual void EmitFileDirective(StringRef Filename);
  virtual void EmitDwarfFileDirective(unsigned FileNo, StringRef Filename) {
    errs() << "FIXME: MCELFStreamer:EmitDwarfFileDirective not implemented\n";
  }

  virtual void EmitInstruction(const MCInst &Inst);
  virtual void Finish();

  /// @}
};

} // end anonymous namespace.

void MCELFStreamer::SwitchSection(const MCSection *Section) {
  assert(Section && "Cannot switch to a null section!");

  // If already in this section, then this is a noop.
  if (Section == CurSection) return;

  CurSection = Section;
  CurSectionData = &Assembler.getOrCreateSectionData(*Section);
}

void MCELFStreamer::EmitLabel(MCSymbol *Symbol) {
  assert(Symbol->isUndefined() && "Cannot define a symbol twice!");

  // FIXME: This is wasteful, we don't necessarily need to create a data
  // fragment. Instead, we should mark the symbol as pointing into the data
  // fragment if it exists, otherwise we should just queue the label and set its
  // fragment pointer when we emit the next fragment.
  MCDataFragment *F = getOrCreateDataFragment();
  MCSymbolData &SD = Assembler.getOrCreateSymbolData(*Symbol);
  assert(!SD.getFragment() && "Unexpected fragment on symbol data!");
  SD.setFragment(F);
  SD.setOffset(F->getContents().size());

  Symbol->setSection(*CurSection);
}

void MCELFStreamer::EmitAssemblerFlag(MCAssemblerFlag Flag) {
  switch (Flag) {
  case MCAF_SubsectionsViaSymbols:
    Assembler.setSubsectionsViaSymbols(true);
    return;
  }

  assert(0 && "invalid assembler flag!");
}

void MCELFStreamer::EmitAssignment(MCSymbol *Symbol, const MCExpr *Value) {
  // Only absolute symbols can be redefined.
  assert((Symbol->isUndefined() || Symbol->isAbsolute()) &&
         "Cannot define a symbol twice!");

  // FIXME: Lift context changes into super class.
  // FIXME: Set associated section.
  //  Symbol->setValue(AddValueSymbols(Value));
}

void MCELFStreamer::EmitSymbolAttribute(MCSymbol *Symbol,
                                          MCSymbolAttr Attribute) {
  // Indirect symbols are handled differently, to match how 'as' handles
  // them. This makes writing matching .o files easier.
  if (Attribute == MCSA_IndirectSymbol) {
    // Note that we intentionally cannot use the symbol data here; this is
    // important for matching the string table that 'as' generates.
    IndirectSymbolData ISD;
    ISD.Symbol = Symbol;
    ISD.SectionData = CurSectionData;
    Assembler.getIndirectSymbols().push_back(ISD);
    return;
  }

  // Adding a symbol attribute always introduces the symbol, note that an
  // important side effect of calling getOrCreateSymbolData here is to register
  // the symbol with the assembler.
  MCSymbolData &SD = Assembler.getOrCreateSymbolData(*Symbol);

  // The implementation of symbol attributes is designed to match 'as', but it
  // leaves much to desired. It doesn't really make sense to arbitrarily add and
  // remove flags, but 'as' allows this (in particular, see .desc).
  //
  // In the future it might be worth trying to make these operations more well
  // defined.
  switch (Attribute) {
  case MCSA_LazyReference:
  case MCSA_Reference:
  case MCSA_NoDeadStrip:
  case MCSA_PrivateExtern:
  case MCSA_WeakReference:
  case MCSA_WeakDefinition:
  case MCSA_Invalid:
  case MCSA_ELF_TypeIndFunction:
  case MCSA_IndirectSymbol:
    assert(0 && "Invalid symbol attribute for ELF!");
    break;

  case MCSA_Global:
    SD.setFlags(SD.getFlags() | (ELF::STB_GLOBAL << ELF::STB_SHIFT));
    SD.setExternal(true);
    break;

  case MCSA_Weak:
    SD.setFlags(SD.getFlags() | (ELF::STB_WEAK << ELF::STB_SHIFT));
    break;

  case MCSA_Local:
    SD.setFlags(SD.getFlags() | (ELF::STB_LOCAL << ELF::STB_SHIFT));
    break;

  case MCSA_ELF_TypeFunction:
    SD.setFlags(SD.getFlags() | (ELF::STT_FUNC << ELF::STT_SHIFT));
    break;

  case MCSA_ELF_TypeObject:
    SD.setFlags(SD.getFlags() | (ELF::STT_OBJECT << ELF::STT_SHIFT));
    break;

  case MCSA_ELF_TypeTLS:
    SD.setFlags(SD.getFlags() | (ELF::STT_TLS << ELF::STT_SHIFT));
    break;

  case MCSA_ELF_TypeCommon:
    SD.setFlags(SD.getFlags() | (ELF::STT_COMMON << ELF::STT_SHIFT));
    break;

  case MCSA_ELF_TypeNoType:
    SD.setFlags(SD.getFlags() | (ELF::STT_NOTYPE << ELF::STT_SHIFT));
    break;

  case MCSA_Protected:
    SD.setFlags(SD.getFlags() | (ELF::STV_PROTECTED << ELF::STV_SHIFT));
    break;

  case MCSA_Hidden:
    SD.setFlags(SD.getFlags() | (ELF::STV_HIDDEN << ELF::STV_SHIFT));
    break;

  case MCSA_Internal:
    SD.setFlags(SD.getFlags() | (ELF::STV_INTERNAL << ELF::STV_SHIFT));
    break;
  }
}

void MCELFStreamer::EmitSymbolDesc(MCSymbol *Symbol, unsigned DescValue) {
}

void MCELFStreamer::EmitCommonSymbol(MCSymbol *Symbol, uint64_t Size,
                                       unsigned ByteAlignment) {
  MCSymbolData &SD = Assembler.getSymbolData(*Symbol);

  if ((SD.getFlags() & (0xff << ELF::STB_SHIFT)) == ELF::STB_LOCAL << ELF::STB_SHIFT) {
    const MCSection *Section = Assembler.getContext().getELFSection(".bss",
                                                                    MCSectionELF::SHT_NOBITS,
                                                                    MCSectionELF::SHF_WRITE |
                                                                    MCSectionELF::SHF_ALLOC,
                                                                    SectionKind::getBSS());

    MCSectionData &SectData = Assembler.getOrCreateSectionData(*Section);
    MCFragment *F = new MCFillFragment(0, 0, Size, &SectData);
    SD.setFragment(F);
    Symbol->setSection(*Section);
  } else {
    SD.setExternal(true);
  }

  SD.setCommon(Size, ByteAlignment);
}

void MCELFStreamer::EmitZerofill(const MCSection *Section, MCSymbol *Symbol,
                                   unsigned Size, unsigned ByteAlignment) {
  MCSectionData &SectData = Assembler.getOrCreateSectionData(*Section);

  // The symbol may not be present, which only creates the section.
  if (!Symbol)
    return;

  // FIXME: Assert that this section has the zerofill type.

  assert(Symbol->isUndefined() && "Cannot define a symbol twice!");

  MCSymbolData &SD = Assembler.getOrCreateSymbolData(*Symbol);

  MCFragment *F = new MCFillFragment(0, 0, Size, &SectData);
  SD.setFragment(F);

  Symbol->setSection(*Section);

  // Update the maximum alignment on the zero fill section if necessary.
  if (ByteAlignment > SectData.getAlignment())
    SectData.setAlignment(ByteAlignment);
}

// This should always be called with the thread local bss section.  Like the
// .zerofill directive this doesn't actually switch sections on us.
void MCELFStreamer::EmitTBSSSymbol(const MCSection *Section, MCSymbol *Symbol,
                                   uint64_t Size, unsigned ByteAlignment) {
  EmitZerofill(Section, Symbol, Size, ByteAlignment);
}

void MCELFStreamer::EmitBytes(StringRef Data, unsigned AddrSpace) {
  getOrCreateDataFragment()->getContents().append(Data.begin(), Data.end());
}

void MCELFStreamer::EmitValue(const MCExpr *Value, unsigned Size,
                                unsigned AddrSpace) {
  MCDataFragment *DF = getOrCreateDataFragment();

  // Avoid fixups when possible.
  int64_t AbsValue;
  if (AddValueSymbols(Value)->EvaluateAsAbsolute(AbsValue)) {
    // FIXME: Endianness assumption.
    for (unsigned i = 0; i != Size; ++i)
      DF->getContents().push_back(uint8_t(AbsValue >> (i * 8)));
  } else {
    DF->addFixup(MCFixup::Create(DF->getContents().size(), AddValueSymbols(Value),
                                 MCFixup::getKindForSize(Size)));
    DF->getContents().resize(DF->getContents().size() + Size, 0);
  }
}

void MCELFStreamer::EmitValueToAlignment(unsigned ByteAlignment,
                                           int64_t Value, unsigned ValueSize,
                                           unsigned MaxBytesToEmit) {
  if (MaxBytesToEmit == 0)
    MaxBytesToEmit = ByteAlignment;
  new MCAlignFragment(ByteAlignment, Value, ValueSize, MaxBytesToEmit,
                      CurSectionData);

  // Update the maximum alignment on the current section if necessary.
  if (ByteAlignment > CurSectionData->getAlignment())
    CurSectionData->setAlignment(ByteAlignment);
}

void MCELFStreamer::EmitCodeAlignment(unsigned ByteAlignment,
                                        unsigned MaxBytesToEmit) {
  if (MaxBytesToEmit == 0)
    MaxBytesToEmit = ByteAlignment;
  new MCAlignFragment(ByteAlignment, 0, 1, MaxBytesToEmit,
                      CurSectionData);

  // Update the maximum alignment on the current section if necessary.
  if (ByteAlignment > CurSectionData->getAlignment())
    CurSectionData->setAlignment(ByteAlignment);
}

void MCELFStreamer::EmitValueToOffset(const MCExpr *Offset,
                                        unsigned char Value) {
  new MCOrgFragment(*Offset, Value, CurSectionData);
}

// Add a symbol for the file name of this module. This is the second
// entry in the module's symbol table (the first being the null symbol).
void MCELFStreamer::EmitFileDirective(StringRef Filename) {
  MCSymbol *Symbol = Assembler.getContext().GetOrCreateSymbol(Filename);
  Symbol->setSection(*CurSection);
  Symbol->setAbsolute();

  MCSymbolData &SD = Assembler.getOrCreateSymbolData(*Symbol);

  SD.setFlags(ELF::STT_FILE << ELF::STT_SHIFT);
  SD.setFlags(SD.getFlags() | (ELF::STB_LOCAL << ELF::STB_SHIFT));
  SD.setFlags(SD.getFlags() | (ELF::STV_DEFAULT << ELF::STV_SHIFT));

}

void MCELFStreamer::EmitInstruction(const MCInst &Inst) {
  // Scan for values.
  for (unsigned i = 0; i != Inst.getNumOperands(); ++i)
    if (Inst.getOperand(i).isExpr())
      AddValueSymbols(Inst.getOperand(i).getExpr());

  CurSectionData->setHasInstructions(true);

  // FIXME-PERF: Common case is that we don't need to relax, encode directly
  // onto the data fragments buffers.

  SmallVector<MCFixup, 4> Fixups;
  SmallString<256> Code;
  raw_svector_ostream VecOS(Code);
  Assembler.getEmitter().EncodeInstruction(Inst, VecOS, Fixups);
  VecOS.flush();

  // FIXME: Eliminate this copy.
  SmallVector<MCFixup, 4> AsmFixups;
  for (unsigned i = 0, e = Fixups.size(); i != e; ++i) {
    MCFixup &F = Fixups[i];
    AsmFixups.push_back(MCFixup::Create(F.getOffset(), F.getValue(),
                                        F.getKind()));
  }

  // See if we might need to relax this instruction, if so it needs its own
  // fragment.
  //
  // FIXME-PERF: Support target hook to do a fast path that avoids the encoder,
  // when we can immediately tell that we will get something which might need
  // relaxation (and compute its size).
  //
  // FIXME-PERF: We should also be smart about immediately relaxing instructions
  // which we can already show will never possibly fit (we can also do a very
  // good job of this before we do the first relaxation pass, because we have
  // total knowledge about undefined symbols at that point). Even now, though,
  // we can do a decent job, especially on Darwin where scattering means that we
  // are going to often know that we can never fully resolve a fixup.
  if (Assembler.getBackend().MayNeedRelaxation(Inst)) {
    MCInstFragment *IF = new MCInstFragment(Inst, CurSectionData);

    // Add the fixups and data.
    //
    // FIXME: Revisit this design decision when relaxation is done, we may be
    // able to get away with not storing any extra data in the MCInst.
    IF->getCode() = Code;
    IF->getFixups() = AsmFixups;

    return;
  }

  // Add the fixups and data.
  MCDataFragment *DF = getOrCreateDataFragment();
  for (unsigned i = 0, e = AsmFixups.size(); i != e; ++i) {
    AsmFixups[i].setOffset(AsmFixups[i].getOffset() + DF->getContents().size());
    DF->addFixup(AsmFixups[i]);
  }
  DF->getContents().append(Code.begin(), Code.end());
}

void MCELFStreamer::Finish() {
  Assembler.Finish();
}

MCStreamer *llvm::createELFStreamer(MCContext &Context, TargetAsmBackend &TAB,
                                      raw_ostream &OS, MCCodeEmitter *CE,
                                      bool RelaxAll) {
  MCELFStreamer *S = new MCELFStreamer(Context, TAB, OS, CE);
  if (RelaxAll)
    S->getAssembler().setRelaxAll(true);
  return S;
}
