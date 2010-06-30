//===- ELFAsmParser.cpp - Parser for ELF Assembly Files---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class implements the parser for ELF assembly files.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Twine.h"
#include "llvm/MC/MCParser/ELFAsmParser.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSymbol.h"

using namespace llvm;

bool ELFAsmParser::ParseDirective(AsmToken DirectiveID) {
  StringRef IDVal = DirectiveID.getIdentifier();
  SMLoc IDLoc = DirectiveID.getLoc();

  if (IDVal == ".local")
    return ParseDirectiveLocal();
  if (IDVal == ".lcomm")
    return ParseDirectiveComm(/*IsLocal=*/true);
  if (IDVal == ".comm")
    return ParseDirectiveComm(/*IsLocal=*/false);

  if (IDVal == ".size")
    return ParseDirectiveSize();

  if (!ParseDirectiveSectionSwitch(IDVal))
    return false;

  // Don't understand directive.
  return true;
}

/// ParseDirectiveSectionSwitch -
bool ELFAsmParser::ParseDirectiveSectionSwitch(StringRef S) {
  unsigned Type = 0;
  unsigned Flags = 0;
  SectionKind Kind;

  if (S == ".text") {
    Type = MCSectionELF::SHT_PROGBITS;
    Flags = MCSectionELF::SHF_EXECINSTR | MCSectionELF::SHF_ALLOC;
    Kind = SectionKind::getText();
  } else if (S == ".bss") {
    Type = MCSectionELF::SHT_NOBITS;
    Flags = MCSectionELF::SHF_WRITE | MCSectionELF::SHF_ALLOC;
    Kind = SectionKind::getBSS();
  } else if (S == ".data") {
    Type = MCSectionELF::SHT_PROGBITS;
    Flags = MCSectionELF::SHF_WRITE | MCSectionELF::SHF_ALLOC;
    Kind = SectionKind::getDataRel();
  } else if (S == ".rodata") {
    Type = MCSectionELF::SHT_PROGBITS;
    Flags = MCSectionELF::SHF_ALLOC;
    Kind = SectionKind::getReadOnly();
  } else if (S == ".tdata") {
    Type = MCSectionELF::SHT_PROGBITS;
    Flags = MCSectionELF::SHF_ALLOC | MCSectionELF::SHF_TLS |
      MCSectionELF::SHF_WRITE;
    Kind = SectionKind::getThreadData();
  } else if (S == ".tbss") {
    Type = MCSectionELF::SHT_NOBITS;
    Flags = MCSectionELF::SHF_ALLOC | MCSectionELF::SHF_TLS |
      MCSectionELF::SHF_WRITE;
    Kind = SectionKind::getThreadBSS();
  } else if (S == ".data.rel") {
    Type = MCSectionELF::SHT_PROGBITS;
    Flags = MCSectionELF::SHF_ALLOC | MCSectionELF::SHF_WRITE;
    Kind = SectionKind::getDataRel();
  } else if (S == ".data.rel.local") {
    Type = MCSectionELF::SHT_PROGBITS;
    Flags = MCSectionELF::SHF_ALLOC | MCSectionELF::SHF_WRITE;
    Kind = SectionKind::getDataRelLocal();
  } else if (S == ".data.rel.ro") {
    Type = MCSectionELF::SHT_PROGBITS;
    Flags = MCSectionELF::SHF_ALLOC | MCSectionELF::SHF_WRITE;
    Kind = SectionKind::getReadOnlyWithRel();
  } else if (S == ".data.rel.ro.local") {
    Type = MCSectionELF::SHT_PROGBITS;
    Flags = MCSectionELF::SHF_ALLOC | MCSectionELF::SHF_WRITE;
    Kind = SectionKind::getReadOnlyWithRelLocal();
  } else if (S == ".eh_frame") {
    Type = MCSectionELF::SHT_PROGBITS;
    Flags = MCSectionELF::SHF_ALLOC | MCSectionELF::SHF_WRITE;
    Kind = SectionKind::getDataRel();
  } else {
    return true;
  }

  // FIXME: We need to handle alignment for sections.
  Out.SwitchSection(Ctx.getELFSection(S, Type, Flags, Kind));

  return false;
}

/// ParseDirectiveLocal
///  ::= .local identifier [ , identifiers ]
bool ELFAsmParser::ParseDirectiveLocal() {
  SMLoc IDLoc = Lexer.getLoc();
  StringRef Name;

  while (1) {
    if (ParseIdentifier(Name))
      return TokError("expected identifier in directive");

    // Handle the identifier as the key symbol.
    MCSymbol *Sym = CreateSymbol(Name);

    Out.EmitSymbolAttribute(Sym, MCSA_Local);

    // Read all symbol names.
    if (Lexer.is(AsmToken::EndOfStatement))
      return false;

    if (Lexer.isNot(AsmToken::Comma))
      return TokError("unexpected token in directive");
    Lex();
  }

  return true;
}

/// ParseDirectiveComm
///  ::= ( .comm | .lcomm ) identifier , size_expression [ , align_expression ]
bool ELFAsmParser::ParseDirectiveComm(bool IsLocal) {
  SMLoc IDLoc = Lexer.getLoc();
  StringRef Name;
  if (ParseIdentifier(Name))
    return TokError("expected identifier in directive");

  // Handle the identifier as the key symbol.
  MCSymbol *Sym = CreateSymbol(Name);

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in directive");
  Lex();

  int64_t Size;
  SMLoc SizeLoc = Lexer.getLoc();
  if (ParseAbsoluteExpression(Size))
    return true;

  int64_t Alignment = 0;
  SMLoc AlignmentLoc;
  if (Lexer.is(AsmToken::Comma)) {
    Lex();
    AlignmentLoc = Lexer.getLoc();
    if (ParseAbsoluteExpression(Alignment))
      return true;

    // For ELF the alignment is in bytes and must be a power of 2.
    if (!isPowerOf2_64(Alignment))
        return Error(AlignmentLoc, "alignment must be a power of 2");
  }

  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.comm' or '.lcomm' directive");

  Lex();

  // NOTE: a size of zero for a .comm should create a undefined symbol
  // but a size of .lcomm creates a bss symbol of size zero.
  if (Size < 0)
    return Error(SizeLoc, "invalid '.comm' or '.lcomm' directive size, can't "
                 "be less than zero");

  // NOTE: The alignment in the directive is a power of 2 value, the assembler
  // may internally end up wanting an alignment in bytes.
  // FIXME: Diagnose overflow.
  if (Alignment < 0)
    return Error(AlignmentLoc, "invalid '.comm' or '.lcomm' directive "
                 "alignment, can't be less than zero");

  if (!Sym->isUndefined())
    return Error(IDLoc, "invalid symbol redefinition");

  // '.lcomm' is equivalent to '.zerofill'.
  // Create the Symbol as a common or local common with Size and Alignment
  if (IsLocal) {
    Out.EmitZerofill(Ctx.getELFSection(".bss", MCSectionELF::SHT_NOBITS,
				       MCSectionELF::SHF_WRITE |
				       MCSectionELF::SHF_ALLOC,
				       SectionKind::getBSS()),
                     Sym, Size, 1 << Alignment);
    return false;
  }

  Out.EmitCommonSymbol(Sym, Size, 1 << Alignment);
  return false;
}

/// ParseDirectiveSize
///  ::= .size identifier , size_expression
bool ELFAsmParser::ParseDirectiveSize() {
  SMLoc IDLoc = Lexer.getLoc();
  StringRef Name;

  if (ParseIdentifier(Name))
    return TokError("expected identifier in directive");

  // Handle the identifier as the key symbol.
  MCSymbol *Sym = CreateSymbol(Name);

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in directive");
  Lex();

  const MCExpr *Value;
  SMLoc SizeLoc = Lexer.getLoc();
  if (ParseExpression(Value))
    return true;

  Out.EmitELFSize(Sym, Value);
  return false;
}

const MCSection *ELFAsmParser::getInitialTextSection() {
  return getContext().getELFSection(".text", MCSectionELF::SHT_PROGBITS,
                                    MCSectionELF::SHF_EXECINSTR |
                                    MCSectionELF::SHF_ALLOC,
                                    SectionKind::getText());
}
