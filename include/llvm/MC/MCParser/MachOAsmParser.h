//===- MachOAsmParser.h - Parser for Mach-O Assembly Files ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class declares the parser for Mach-O assembly files.
//
//===----------------------------------------------------------------------===//

#ifndef MACHOASMPARSER_H
#define MACHOASMPARSER_H

#include "llvm/MC/MCParser/AsmParser.h"

namespace llvm {
class MCContext;
class MCStreamer;
class MCAsmInfo;
class SourceMgr;

class MachOAsmParser : public AsmParser {
public:
  MachOAsmParser(SourceMgr &SM, MCContext &Ctx, MCStreamer &Out,
                 const MCAsmInfo &MAI)
    : AsmParser(SM, Ctx, Out, MAI) {}

  ~MachOAsmParser() {};

  /// ParseDirectiveSectionSwitch - Parse Mach-O specific asssembler
  /// section directives.
  bool ParseDirectiveSectionSwitch(StringRef Section);
  bool ParseDirectiveDarwinSection(); // Darwin specific ".section".

  bool ParseDirectiveDarwinSymbolDesc(); // Darwin specific ".desc"
  bool ParseDirectiveDarwinLsym(); // Darwin specific ".lsym"

  bool ParseDirectiveDarwinZerofill(); // Darwin specific ".zerofill"
  bool ParseDirectiveDarwinTBSS(); // Darwin specific ".tbss"

  // Darwin specific ".subsections_via_symbols"
  bool ParseDirectiveDarwinSubsectionsViaSymbols();
  // Darwin specific .dump and .load
  bool ParseDirectiveDarwinDumpOrLoad(SMLoc IDLoc, bool IsDump);

  bool ParseDirectiveComm(bool IsLocal);
  bool ParseDirective(AsmToken DirectiveID);
  const MCSection *getInitialTextSection();
};

} // end namespace llvm

#endif
