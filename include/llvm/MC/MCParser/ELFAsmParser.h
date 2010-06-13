//===- ELFAsmParser.h - Parser for ELF Assembly Files ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class declares the parser for ELF assembly files.
//
//===----------------------------------------------------------------------===//

#ifndef ELFASMPARSER_H
#define ELFASMPARSER_H

#include "llvm/MC/MCParser/AsmParser.h"

namespace llvm {
class MCContext;
class MCStreamer;
class MCAsmInfo;
class SourceMgr;

class ELFAsmParser : public AsmParser {
public:
  ELFAsmParser(SourceMgr &SM, MCContext &Ctx, MCStreamer &Out,
                 const MCAsmInfo &MAI)
    : AsmParser(SM, Ctx, Out, MAI) {}

  ~ELFAsmParser() {};

  /// ParseDirectiveSectionSwitch - Parse ELF specific asssembler
  /// section directives.
  bool ParseDirectiveSectionSwitch(StringRef Section);
  bool ParseDirective(AsmToken DirectiveID);

  bool ParseDirectiveComm(bool IsLocal);
  bool ParseDirectiveLocal();
  bool ParseDirectiveSize();
  const MCSection *getInitialTextSection();
};

} // end namespace llvm

#endif
