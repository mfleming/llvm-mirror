//===- MachOAsmParser.cpp - Parser for Mach-O Assembly Files---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class implements the parser for Mach-O assembly files.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Twine.h"
#include "llvm/MC/MCParser/MachOAsmParser.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSymbol.h"

using namespace llvm;

bool MachOAsmParser::ParseDirective(AsmToken DirectiveID) {
  StringRef IDVal = DirectiveID.getIdentifier();
  SMLoc IDLoc = DirectiveID.getLoc();

  // FIXME: This should be driven based on a hash lookup and callback.
  if (IDVal == ".section")
    return ParseDirectiveDarwinSection();

  if (IDVal == ".zerofill")
    return ParseDirectiveDarwinZerofill();
  if (IDVal == ".desc")
    return ParseDirectiveDarwinSymbolDesc();
  if (IDVal == ".lsym")
    return ParseDirectiveDarwinLsym();
  if (IDVal == ".tbss")
    return ParseDirectiveDarwinTBSS();

  if (IDVal == ".comm")
    return ParseDirectiveComm(/*IsLocal=*/false);
  if (IDVal == ".lcomm")
    return ParseDirectiveComm(/*IsLocal=*/true);

  if (IDVal == ".subsections_via_symbols")
    return ParseDirectiveDarwinSubsectionsViaSymbols();
  if (IDVal == ".dump")
    return ParseDirectiveDarwinDumpOrLoad(IDLoc, /*IsDump=*/true);
  if (IDVal == ".load")
    return ParseDirectiveDarwinDumpOrLoad(IDLoc, /*IsLoad=*/false);

  if (!ParseDirectiveSectionSwitch(IDVal))
    return false;

  // Don't understand directive.
  return true;
}

/// ParseDirectiveSectionSwitch -
bool MachOAsmParser::ParseDirectiveSectionSwitch(StringRef S) {
  const char *Segment;
  const char *Section;
  unsigned TAA = 0;
  unsigned Align = 0;
  unsigned StubSize = 0;

  if (S == ".text") {
    Segment = "__TEXT";
    Section = "__text";
    TAA = MCSectionMachO::S_ATTR_PURE_INSTRUCTIONS;
  } else if (S == ".const") {
    Segment = "__TEXT";
    Section = "__const";
  } else if (S == ".static_const") {
    Segment = "__TEXT";
    Section = "__static_const";
  } else if (S == ".cstring") {
    Segment = "__TEXT";
    Section = "__cstring";
    TAA = MCSectionMachO::S_CSTRING_LITERALS;
  } else if (S == ".literal4") {
    Segment = "__TEXT";
    Section = "__literal4";
    TAA = MCSectionMachO::S_4BYTE_LITERALS;
    Align = 4;
  } else if (S == ".literal8") {
    Segment = "__TEXT";
    Section = "__literal8";
    TAA = MCSectionMachO::S_8BYTE_LITERALS;
    Align = 8;
  } else if (S == ".literal16") {
    Segment = "__TEXT";
    Section = "__literal16";
    TAA = MCSectionMachO::S_16BYTE_LITERALS;
    Align = 16;
  } else if (S == ".constructor") {
    Segment = "__TEXT";
    Section = "__constructor";
  } else if (S == ".destructor") {
    Segment = "__TEXT";
    Section = "__destructor";
  } else if (S == ".fvmlib_init0") {
    Segment = "__TEXT";
    Section = "__fvmlib_init0";
  } else if (S == ".fvmlib_init1") {
    Segment = "__TEXT";
    Section = "__fvmlib_init1";

    // FIXME: The assembler manual claims that this has the self modify code
    // flag, at least on x86-32, but that does not appear to be correct.
  } else if (S == ".symbol_stub") {
    Segment = "__TEXT";
    Section = "__symbol_stub";
    TAA = MCSectionMachO::S_SYMBOL_STUBS |
      MCSectionMachO::S_ATTR_PURE_INSTRUCTIONS;
    // FIXME: Different on PPC and ARM.
    StubSize = 16;
    // FIXME: PowerPC only?
  } else if (S == ".picsymbol_stub") {
    Segment = "__TEXT";
    Section = "__picsymbol_stub";
    TAA = MCSectionMachO::S_SYMBOL_STUBS |
      MCSectionMachO::S_ATTR_PURE_INSTRUCTIONS;
    StubSize = 26;
  } else if (S == ".data") {
    Segment = "__DATA";
    Section = "__data";
  } else if (S == ".static_data") {
    Segment = "__DATA";
    Section = "__static_data";

    // FIXME: The section names of these two are misspelled in the assembler
    // manual.
  } else if (S == ".non_lazy_symbol_pointer") {
    Segment = "__DATA";
    Section = "__nl_symbol_ptr";
    TAA = MCSectionMachO::S_NON_LAZY_SYMBOL_POINTERS;
    Align = 4;
  } else if (S == ".lazy_symbol_pointer") {
    Segment = "__DATA";
    Section = "__la_symbol_ptr";
    TAA = MCSectionMachO::S_LAZY_SYMBOL_POINTERS;
    Align = 4;
  } else if (S == ".dyld") {
    Segment = "__DATA";
    Section = "__dyld";
  } else if (S == ".mod_init_func") {
    Segment = "__DATA";
    Section = "__mod_init_func";
    TAA = MCSectionMachO::S_MOD_INIT_FUNC_POINTERS;
    Align = 4;
  } else if (S == ".mod_term_func") {
    Segment = "__DATA";
    Section = "__mod_term_func";
    TAA = MCSectionMachO::S_MOD_TERM_FUNC_POINTERS;
    Align = 4;
  } else if (S == ".const_data") {
    Segment = "__DATA";
    Section = "__const";
  } else if (S == ".objc_class") {
    Segment = "__OBJC";
    Section = "__class";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP;
  } else if (S == ".objc_meta_class") {
    Segment = "__OBJC";
    Section = "__meta_class";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP;
  } else if (S == ".objc_cat_cls_meth") {
    Segment = "__OBJC";
    Section = "__cat_cls_meth";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP;
  } else if (S == ".objc_cat_inst_meth") {
    Segment = "__OBJC";
    Section = "__cat_inst_meth";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP;
  } else if (S == ".objc_protocol") {
    Segment = "__OBJC";
    Section = "__protocol";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP;
  } else if (S == ".objc_string_object") {
    Segment = "__OBJC";
    Section = "__string_object";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP;
  } else if (S == ".objc_cls_meth") {
    Segment = "__OBJC";
    Section = "__cls_meth";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP;
  } else if (S == ".objc_inst_meth") {
    Segment = "__OBJC";
    Section = "__inst_meth";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP;
  } else if (S == ".objc_cls_refs") {
    Segment = "__OBJC";
    Section = "__cls_refs";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP |
      MCSectionMachO::S_LITERAL_POINTERS;
    Align = 4;
  } else if (S == ".objc_message_refs") {
    Segment = "__OBJC";
    Section = "__message_refs";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP |
      MCSectionMachO::S_LITERAL_POINTERS;
    Align = 4;
  } else if (S == ".objc_symbols") {
    Segment = "__OBJC";
    Section = "__symbols";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP;
  } else if (S == ".objc_category") {
    Segment = "__OBJC";
    Section = "__category";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP;
  } else if (S == ".objc_class_vars") {
    Segment = "__OBJC";
    Section = "__class_vars";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP;
  } else if (S == ".objc_instance_vars") {
    Segment = "__OBJC";
    Section = "__instance_vars";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP;
  } else if (S == ".objc_module_info") {
    Segment = "__OBJC";
    Section = "__module_info";
    TAA = MCSectionMachO::S_ATTR_NO_DEAD_STRIP;
  } else if (S == ".objc_class_names") {
    Segment = "__TEXT";
    Section = "__cstring";
    TAA = MCSectionMachO::S_CSTRING_LITERALS;
  } else if (S == ".objc_meth_var_types") {
    Segment = "__TEXT";
    Section = "__cstring";
    TAA = MCSectionMachO::S_CSTRING_LITERALS;
  } else if (S == ".objc_meth_var_names") {
    Segment = "__TEXT";
    Section = "__cstring";
    TAA = MCSectionMachO::S_CSTRING_LITERALS;
  } else if (S == ".objc_selector_strs") {
    Segment = "__OBJC";
    Section = "__selector_strs";
    TAA = MCSectionMachO::S_CSTRING_LITERALS;
  } else if (S == ".tdata") {
    Segment = "__DATA";
    Section = "__thread_data";
    TAA = MCSectionMachO::S_THREAD_LOCAL_REGULAR;
    } else if (S == ".tlv") {
    Segment = "__DATA";
    Section = "__thread_vars";
    TAA = MCSectionMachO::S_THREAD_LOCAL_VARIABLES;
  } else if (S == ".thread_init_func") {
    Segment = "__DATA";
    Section = "__thread_init";
    TAA = MCSectionMachO::S_THREAD_LOCAL_INIT_FUNCTION_POINTERS;
  } else {
    return true;
  }

  bool isText = StringRef(Segment) == "__TEXT";  // FIXME: Hack.
  Out.SwitchSection(Ctx.getMachOSection(Segment, Section, TAA, StubSize,
                                        isText ? SectionKind::getText()
                                               : SectionKind::getDataRel()));

  // Set the implicit alignment, if any.
  //
  // FIXME: This isn't really what 'as' does; I think it just uses the implicit
  // alignment on the section (e.g., if one manually inserts bytes into the
  // section, then just issueing the section switch directive will not realign
  // the section. However, this is arguably more reasonable behavior, and there
  // is no good reason for someone to intentionally emit incorrectly sized
  // values into the implicitly aligned sections.
  if (Align)
    Out.EmitValueToAlignment(Align, 0, 1, 0);

  return false;
}

/// ParseDirectiveSection:
///   ::= .section identifier (',' identifier)*
/// FIXME: This should actually parse out the segment, section, attributes and
/// sizeof_stub fields.
bool MachOAsmParser::ParseDirectiveDarwinSection() {
  SMLoc Loc = Lexer.getLoc();

  StringRef SectionName;
  if (ParseIdentifier(SectionName))
    return Error(Loc, "expected identifier after '.section' directive");

  // Verify there is a following comma.
  if (!Lexer.is(AsmToken::Comma))
    return TokError("unexpected token in '.section' directive");

  std::string SectionSpec = SectionName;
  SectionSpec += ",";

  // Add all the tokens until the end of the line, ParseSectionSpecifier will
  // handle this.
  StringRef EOL = Lexer.LexUntilEndOfStatement();
  SectionSpec.append(EOL.begin(), EOL.end());

  Lex();
  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.section' directive");
  Lex();


  StringRef Segment, Section;
  unsigned TAA, StubSize;
  std::string ErrorStr =
    MCSectionMachO::ParseSectionSpecifier(SectionSpec, Segment, Section,
                                          TAA, StubSize);

  if (!ErrorStr.empty())
    return Error(Loc, ErrorStr.c_str());

  bool isText = Segment == "__TEXT";  // FIXME: Hack.
  Out.SwitchSection(Ctx.getMachOSection(Segment, Section, TAA, StubSize,
                                        isText ? SectionKind::getText()
                                               : SectionKind::getDataRel()));
  return false;
}

/// ParseDirectiveDarwinZerofill
///  ::= .zerofill segname , sectname [, identifier , size_expression [
///      , align_expression ]]
bool MachOAsmParser::ParseDirectiveDarwinZerofill() {
  StringRef Segment;
  if (ParseIdentifier(Segment))
    return TokError("expected segment name after '.zerofill' directive");

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in directive");
  Lex();

  StringRef Section;
  if (ParseIdentifier(Section))
    return TokError("expected section name after comma in '.zerofill' "
                    "directive");

  // If this is the end of the line all that was wanted was to create the
  // the section but with no symbol.
  if (Lexer.is(AsmToken::EndOfStatement)) {
    // Create the zerofill section but no symbol
    Out.EmitZerofill(Ctx.getMachOSection(Segment, Section,
                                         MCSectionMachO::S_ZEROFILL, 0,
                                         SectionKind::getBSS()));
    return false;
  }

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in directive");
  Lex();

  SMLoc IDLoc = Lexer.getLoc();
  StringRef IDStr;
  if (ParseIdentifier(IDStr))
    return TokError("expected identifier in directive");

  // handle the identifier as the key symbol.
  MCSymbol *Sym = CreateSymbol(IDStr);

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in directive");
  Lex();

  int64_t Size;
  SMLoc SizeLoc = Lexer.getLoc();
  if (ParseAbsoluteExpression(Size))
    return true;

  int64_t Pow2Alignment = 0;
  SMLoc Pow2AlignmentLoc;
  if (Lexer.is(AsmToken::Comma)) {
    Lex();
    Pow2AlignmentLoc = Lexer.getLoc();
    if (ParseAbsoluteExpression(Pow2Alignment))
      return true;
  }

  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.zerofill' directive");

  Lex();

  if (Size < 0)
    return Error(SizeLoc, "invalid '.zerofill' directive size, can't be less "
                 "than zero");

  // NOTE: The alignment in the directive is a power of 2 value, the assembler
  // may internally end up wanting an alignment in bytes.
  // FIXME: Diagnose overflow.
  if (Pow2Alignment < 0)
    return Error(Pow2AlignmentLoc, "invalid '.zerofill' directive alignment, "
                 "can't be less than zero");

  if (!Sym->isUndefined())
    return Error(IDLoc, "invalid symbol redefinition");

  // Create the zerofill Symbol with Size and Pow2Alignment
  Out.EmitZerofill(Ctx.getMachOSection(Segment, Section,
                                       MCSectionMachO::S_ZEROFILL, 0,
                                       SectionKind::getBSS()),
                   Sym, Size, 1 << Pow2Alignment);

  return false;
}

/// ParseDirectiveDarwinTBSS
///  ::= .tbss identifier, size, align
bool MachOAsmParser::ParseDirectiveDarwinTBSS() {
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

  int64_t Pow2Alignment = 0;
  SMLoc Pow2AlignmentLoc;
  if (Lexer.is(AsmToken::Comma)) {
    Lex();
    Pow2AlignmentLoc = Lexer.getLoc();
    if (ParseAbsoluteExpression(Pow2Alignment))
      return true;
  }

  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.tbss' directive");

  Lex();

  if (Size < 0)
    return Error(SizeLoc, "invalid '.tbss' directive size, can't be less than"
                 "zero");

  // FIXME: Diagnose overflow.
  if (Pow2Alignment < 0)
    return Error(Pow2AlignmentLoc, "invalid '.tbss' alignment, can't be less"
                 "than zero");

  if (!Sym->isUndefined())
    return Error(IDLoc, "invalid symbol redefinition");

  Out.EmitTBSSSymbol(Ctx.getMachOSection("__DATA", "__thread_bss",
                                        MCSectionMachO::S_THREAD_LOCAL_ZEROFILL,
                                        0, SectionKind::getThreadBSS()),
                     Sym, Size, 1 << Pow2Alignment);

  return false;
}

/// ParseDirectiveDarwinSubsectionsViaSymbols
///  ::= .subsections_via_symbols
bool MachOAsmParser::ParseDirectiveDarwinSubsectionsViaSymbols() {
  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.subsections_via_symbols' directive");

  Lex();

  Out.EmitAssemblerFlag(MCAF_SubsectionsViaSymbols);

  return false;
}

/// ParseDirectiveComm
///  ::= ( .comm | .lcomm ) identifier , size_expression [ , align_expression ]
bool MachOAsmParser::ParseDirectiveComm(bool IsLocal) {
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

  int64_t Pow2Alignment = 0;
  SMLoc Pow2AlignmentLoc;
  if (Lexer.is(AsmToken::Comma)) {
    Lex();
    Pow2AlignmentLoc = Lexer.getLoc();
    if (ParseAbsoluteExpression(Pow2Alignment))
      return true;

    // If this target takes alignments in bytes (not log) validate and convert.
    if (Lexer.getMAI().getAlignmentIsInBytes()) {
      if (!isPowerOf2_64(Pow2Alignment))
        return Error(Pow2AlignmentLoc, "alignment must be a power of 2");
      Pow2Alignment = Log2_64(Pow2Alignment);
    }
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
  if (Pow2Alignment < 0)
    return Error(Pow2AlignmentLoc, "invalid '.comm' or '.lcomm' directive "
                 "alignment, can't be less than zero");

  if (!Sym->isUndefined())
    return Error(IDLoc, "invalid symbol redefinition");

  // '.lcomm' is equivalent to '.zerofill'.
  // Create the Symbol as a common or local common with Size and Pow2Alignment
  if (IsLocal) {
    Out.EmitZerofill(Ctx.getMachOSection("__DATA", "__bss",
                                         MCSectionMachO::S_ZEROFILL, 0,
                                         SectionKind::getBSS()),
                     Sym, Size, 1 << Pow2Alignment);
    return false;
  }

  Out.EmitCommonSymbol(Sym, Size, 1 << Pow2Alignment);
  return false;
}

/// ParseDirectiveDarwinDumpOrLoad
///  ::= ( .dump | .load ) "filename"
bool MachOAsmParser::ParseDirectiveDarwinDumpOrLoad(SMLoc IDLoc, bool IsDump) {
  if (Lexer.isNot(AsmToken::String))
    return TokError("expected string in '.dump' or '.load' directive");

  Lex();

  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.dump' or '.load' directive");

  Lex();

  // FIXME: If/when .dump and .load are implemented they will be done in the
  // the assembly parser and not have any need for an MCStreamer API.
  if (IsDump)
    Warning(IDLoc, "ignoring directive .dump for now");
  else
    Warning(IDLoc, "ignoring directive .load for now");

  return false;
}

/// ParseDirectiveLsym
///  ::= .lsym identifier , expression

bool MachOAsmParser::ParseDirectiveDarwinLsym() {
  StringRef Name;
  if (ParseIdentifier(Name))
    return TokError("expected identifier in directive");

  // Handle the identifier as the key symbol.
  MCSymbol *Sym = CreateSymbol(Name);

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in '.lsym' directive");
  Lex();

  const MCExpr *Value;
  SMLoc StartLoc = Lexer.getLoc();
  if (ParseExpression(Value))
    return true;

  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.lsym' directive");

  Lex();

  // We don't currently support this directive.
  //
  // FIXME: Diagnostic location!
  (void) Sym;
  return TokError("directive '.lsym' is unsupported");
}

/// ParseDirectiveDarwinSymbolDesc
///  ::= .desc identifier , expression
bool MachOAsmParser::ParseDirectiveDarwinSymbolDesc() {
  StringRef Name;
  if (ParseIdentifier(Name))
    return TokError("expected identifier in directive");

  // Handle the identifier as the key symbol.
  MCSymbol *Sym = CreateSymbol(Name);

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in '.desc' directive");
  Lex();

  SMLoc DescLoc = Lexer.getLoc();
  int64_t DescValue;
  if (ParseAbsoluteExpression(DescValue))
    return true;

  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.desc' directive");

  Lex();

  // Set the n_desc field of this Symbol to this DescValue
  Out.EmitSymbolDesc(Sym, DescValue);

  return false;
}

const MCSection *MachOAsmParser::getInitialTextSection() {
  return getContext().getMachOSection("__TEXT", "__text",
                                      MCSectionMachO::S_ATTR_PURE_INSTRUCTIONS,
                                      0, SectionKind::getText());
}
