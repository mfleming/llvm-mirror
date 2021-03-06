//===-- ARMAsmParser.cpp - Parse ARM assembly to MCInst instructions ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ARM.h"
#include "llvm/MC/MCParser/MachOAsmParser.h"
#include "llvm/MC/MCParser/MCAsmLexer.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Target/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
using namespace llvm;

namespace {
struct ARMOperand;

// The shift types for register controlled shifts in arm memory addressing
enum ShiftType {
  Lsl,
  Lsr,
  Asr,
  Ror,
  Rrx
};

class ARMAsmParser : public MachOAsmParser {

private:
  bool MaybeParseRegister(OwningPtr<ARMOperand> &Op, bool ParseWriteBack);

  bool ParseRegisterList(OwningPtr<ARMOperand> &Op);

  bool ParseMemory(OwningPtr<ARMOperand> &Op);

  bool ParseMemoryOffsetReg(bool &Negative,
                            bool &OffsetRegShifted,
                            enum ShiftType &ShiftType,
                            const MCExpr *&ShiftAmount,
                            const MCExpr *&Offset,
                            bool &OffsetIsReg,
                            int &OffsetRegNum,
                            SMLoc &E);

  bool ParseShift(enum ShiftType &St, const MCExpr *&ShiftAmount, SMLoc &E);

  bool ParseOperand(OwningPtr<ARMOperand> &Op);

  bool ParseDirectiveWord(unsigned Size, SMLoc L);

  bool ParseDirectiveThumb(SMLoc L);

  bool ParseDirectiveThumbFunc(SMLoc L);

  bool ParseDirectiveCode(SMLoc L);

  bool ParseDirectiveSyntax(SMLoc L);

  // TODO - For now hacked versions of the next two are in here in this file to
  // allow some parser testing until the table gen versions are implemented.

  /// @name Auto-generated Match Functions
  /// {
  virtual bool MatchInstruction(const SmallVectorImpl<MCParsedAsmOperand*> &Operands,
                                MCInst &Inst);

  /// MatchRegisterName - Match the given string to a register name and return
  /// its register number, or -1 if there is no match.  To allow return values
  /// to be used directly in register lists, arm registers have values between
  /// 0 and 15.
  int MatchRegisterName(const StringRef &Name);

  /// }


public:
  ARMAsmParser(const Target &T, SourceMgr &SM, MCContext &Ctx,
               MCStreamer &Out, const MCAsmInfo &MAI)
    : MachOAsmParser(SM, Ctx, Out, MAI) {}

  virtual bool ParseInstruction(const StringRef &Name, SMLoc NameLoc,
                                SmallVectorImpl<MCParsedAsmOperand*> &Operands);

  virtual bool ParseTargetDirective(AsmToken DirectiveID);
};
  
/// ARMOperand - Instances of this class represent a parsed ARM machine
/// instruction.
struct ARMOperand : public MCParsedAsmOperand {
private:
  ARMOperand() {}
public:
  enum KindTy {
    Token,
    Register,
    Immediate,
    Memory
  } Kind;

  SMLoc StartLoc, EndLoc;

  union {
    struct {
      const char *Data;
      unsigned Length;
    } Tok;

    struct {
      unsigned RegNum;
      bool Writeback;
    } Reg;

    struct {
      const MCExpr *Val;
    } Imm;
    
    // This is for all forms of ARM address expressions
    struct {
      unsigned BaseRegNum;
      unsigned OffsetRegNum; // used when OffsetIsReg is true
      const MCExpr *Offset; // used when OffsetIsReg is false
      const MCExpr *ShiftAmount; // used when OffsetRegShifted is true
      enum ShiftType ShiftType;  // used when OffsetRegShifted is true
      unsigned
        OffsetRegShifted : 1, // only used when OffsetIsReg is true
        Preindexed : 1,
        Postindexed : 1,
        OffsetIsReg : 1,
        Negative : 1, // only used when OffsetIsReg is true
        Writeback : 1;
    } Mem;

  };
  
  ARMOperand(KindTy K, SMLoc S, SMLoc E)
    : Kind(K), StartLoc(S), EndLoc(E) {}
  
  ARMOperand(const ARMOperand &o) : MCParsedAsmOperand() {
    Kind = o.Kind;
    StartLoc = o.StartLoc;
    EndLoc = o.EndLoc;
    switch (Kind) {
    case Token:
    Tok = o.Tok;
      break;
    case Register:
      Reg = o.Reg;
      break;
    case Immediate:
      Imm = o.Imm;
      break;
    case Memory:
      Mem = o.Mem;
      break;
    }
  }
  
  /// getStartLoc - Get the location of the first token of this operand.
  SMLoc getStartLoc() const { return StartLoc; }
  /// getEndLoc - Get the location of the last token of this operand.
  SMLoc getEndLoc() const { return EndLoc; }

  StringRef getToken() const {
    assert(Kind == Token && "Invalid access!");
    return StringRef(Tok.Data, Tok.Length);
  }

  unsigned getReg() const {
    assert(Kind == Register && "Invalid access!");
    return Reg.RegNum;
  }

  const MCExpr *getImm() const {
    assert(Kind == Immediate && "Invalid access!");
    return Imm.Val;
  }

  bool isToken() const {return Kind == Token; }

  bool isReg() const { return Kind == Register; }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1 && "Invalid number of operands!");
    Inst.addOperand(MCOperand::CreateReg(getReg()));
  }

  static void CreateToken(OwningPtr<ARMOperand> &Op, StringRef Str,
                          SMLoc S) {
    Op.reset(new ARMOperand);
    Op->Kind = Token;
    Op->Tok.Data = Str.data();
    Op->Tok.Length = Str.size();
    Op->StartLoc = S;
    Op->EndLoc = S;
  }

  static void CreateReg(OwningPtr<ARMOperand> &Op, unsigned RegNum, 
                        bool Writeback, SMLoc S, SMLoc E) {
    Op.reset(new ARMOperand);
    Op->Kind = Register;
    Op->Reg.RegNum = RegNum;
    Op->Reg.Writeback = Writeback;
    
    Op->StartLoc = S;
    Op->EndLoc = E;
  }

  static void CreateImm(OwningPtr<ARMOperand> &Op, const MCExpr *Val,
                        SMLoc S, SMLoc E) {
    Op.reset(new ARMOperand);
    Op->Kind = Immediate;
    Op->Imm.Val = Val;
    
    Op->StartLoc = S;
    Op->EndLoc = E;
  }

  static void CreateMem(OwningPtr<ARMOperand> &Op,
                        unsigned BaseRegNum, bool OffsetIsReg,
                        const MCExpr *Offset, unsigned OffsetRegNum,
                        bool OffsetRegShifted, enum ShiftType ShiftType,
                        const MCExpr *ShiftAmount, bool Preindexed,
                        bool Postindexed, bool Negative, bool Writeback,
                        SMLoc S, SMLoc E) {
    Op.reset(new ARMOperand);
    Op->Kind = Memory;
    Op->Mem.BaseRegNum = BaseRegNum;
    Op->Mem.OffsetIsReg = OffsetIsReg;
    Op->Mem.Offset = Offset;
    Op->Mem.OffsetRegNum = OffsetRegNum;
    Op->Mem.OffsetRegShifted = OffsetRegShifted;
    Op->Mem.ShiftType = ShiftType;
    Op->Mem.ShiftAmount = ShiftAmount;
    Op->Mem.Preindexed = Preindexed;
    Op->Mem.Postindexed = Postindexed;
    Op->Mem.Negative = Negative;
    Op->Mem.Writeback = Writeback;
    
    Op->StartLoc = S;
    Op->EndLoc = E;
  }
};

} // end anonymous namespace.

/// Try to parse a register name.  The token must be an Identifier when called,
/// and if it is a register name a Reg operand is created, the token is eaten
/// and false is returned.  Else true is returned and no token is eaten.
/// TODO this is likely to change to allow different register types and or to
/// parse for a specific register type.
bool ARMAsmParser::MaybeParseRegister
  (OwningPtr<ARMOperand> &Op, bool ParseWriteBack) {
  SMLoc S, E;
  const AsmToken &Tok = getTok();
  assert(Tok.is(AsmToken::Identifier) && "Token is not an Identifier");

  // FIXME: Validate register for the current architecture; we have to do
  // validation later, so maybe there is no need for this here.
  int RegNum;

  RegNum = MatchRegisterName(Tok.getString());
  if (RegNum == -1)
    return true;
  
  S = Tok.getLoc();
  
  Lex(); // Eat identifier token.
    
  E = getTok().getLoc();

  bool Writeback = false;
  if (ParseWriteBack) {
    const AsmToken &ExclaimTok = getTok();
    if (ExclaimTok.is(AsmToken::Exclaim)) {
      E = ExclaimTok.getLoc();
      Writeback = true;
      Lex(); // Eat exclaim token
    }
  }

  ARMOperand::CreateReg(Op, RegNum, Writeback, S, E);

  return false;
}

/// Parse a register list, return false if successful else return true or an 
/// error.  The first token must be a '{' when called.
bool ARMAsmParser::ParseRegisterList(OwningPtr<ARMOperand> &Op) {
  SMLoc S, E;
  assert(getTok().is(AsmToken::LCurly) &&
         "Token is not an Left Curly Brace");
  S = getTok().getLoc();
  Lex(); // Eat left curly brace token.

  const AsmToken &RegTok = getTok();
  SMLoc RegLoc = RegTok.getLoc();
  if (RegTok.isNot(AsmToken::Identifier))
    return Error(RegLoc, "register expected");
  int RegNum = MatchRegisterName(RegTok.getString());
  if (RegNum == -1)
    return Error(RegLoc, "register expected");
  Lex(); // Eat identifier token.
  unsigned RegList = 1 << RegNum;

  int HighRegNum = RegNum;
  // TODO ranges like "{Rn-Rm}"
  while (getTok().is(AsmToken::Comma)) {
    Lex(); // Eat comma token.

    const AsmToken &RegTok = getTok();
    SMLoc RegLoc = RegTok.getLoc();
    if (RegTok.isNot(AsmToken::Identifier))
      return Error(RegLoc, "register expected");
    int RegNum = MatchRegisterName(RegTok.getString());
    if (RegNum == -1)
      return Error(RegLoc, "register expected");

    if (RegList & (1 << RegNum))
      Warning(RegLoc, "register duplicated in register list");
    else if (RegNum <= HighRegNum)
      Warning(RegLoc, "register not in ascending order in register list");
    RegList |= 1 << RegNum;
    HighRegNum = RegNum;

    Lex(); // Eat identifier token.
  }
  const AsmToken &RCurlyTok = getTok();
  if (RCurlyTok.isNot(AsmToken::RCurly))
    return Error(RCurlyTok.getLoc(), "'}' expected");
  E = RCurlyTok.getLoc();
  Lex(); // Eat left curly brace token.

  return false;
}

/// Parse an arm memory expression, return false if successful else return true
/// or an error.  The first token must be a '[' when called.
/// TODO Only preindexing and postindexing addressing are started, unindexed
/// with option, etc are still to do.
bool ARMAsmParser::ParseMemory(OwningPtr<ARMOperand> &Op) {
  SMLoc S, E;
  assert(getTok().is(AsmToken::LBrac) &&
         "Token is not an Left Bracket");
  S = getTok().getLoc();
  Lex(); // Eat left bracket token.

  const AsmToken &BaseRegTok = getTok();
  if (BaseRegTok.isNot(AsmToken::Identifier))
    return Error(BaseRegTok.getLoc(), "register expected");
  if (MaybeParseRegister(Op, false))
    return Error(BaseRegTok.getLoc(), "register expected");
  int BaseRegNum = Op->getReg();

  bool Preindexed = false;
  bool Postindexed = false;
  bool OffsetIsReg = false;
  bool Negative = false;
  bool Writeback = false;

  // First look for preindexed address forms, that is after the "[Rn" we now
  // have to see if the next token is a comma.
  const AsmToken &Tok = getTok();
  if (Tok.is(AsmToken::Comma)) {
    Preindexed = true;
    Lex(); // Eat comma token.
    int OffsetRegNum;
    bool OffsetRegShifted;
    enum ShiftType ShiftType;
    const MCExpr *ShiftAmount;
    const MCExpr *Offset;
    if(ParseMemoryOffsetReg(Negative, OffsetRegShifted, ShiftType, ShiftAmount,
                            Offset, OffsetIsReg, OffsetRegNum, E))
      return true;
    const AsmToken &RBracTok = getTok();
    if (RBracTok.isNot(AsmToken::RBrac))
      return Error(RBracTok.getLoc(), "']' expected");
    E = RBracTok.getLoc();
    Lex(); // Eat right bracket token.

    const AsmToken &ExclaimTok = getTok();
    if (ExclaimTok.is(AsmToken::Exclaim)) {
      E = ExclaimTok.getLoc();
      Writeback = true;
      Lex(); // Eat exclaim token
    }
    ARMOperand::CreateMem(Op, BaseRegNum, OffsetIsReg, Offset, OffsetRegNum,
                          OffsetRegShifted, ShiftType, ShiftAmount,
                          Preindexed, Postindexed, Negative, Writeback, S, E);
    return false;
  }
  // The "[Rn" we have so far was not followed by a comma.
  else if (Tok.is(AsmToken::RBrac)) {
    // This is a post indexing addressing forms, that is a ']' follows after
    // the "[Rn".
    Postindexed = true;
    Writeback = true;
    E = Tok.getLoc();
    Lex(); // Eat right bracket token.

    int OffsetRegNum = 0;
    bool OffsetRegShifted = false;
    enum ShiftType ShiftType;
    const MCExpr *ShiftAmount;
    const MCExpr *Offset;

    const AsmToken &NextTok = getTok();
    if (NextTok.isNot(AsmToken::EndOfStatement)) {
      if (NextTok.isNot(AsmToken::Comma))
	return Error(NextTok.getLoc(), "',' expected");
      Lex(); // Eat comma token.
      if(ParseMemoryOffsetReg(Negative, OffsetRegShifted, ShiftType,
                              ShiftAmount, Offset, OffsetIsReg, OffsetRegNum, 
                              E))
        return true;
    }

    ARMOperand::CreateMem(Op, BaseRegNum, OffsetIsReg, Offset, OffsetRegNum,
                          OffsetRegShifted, ShiftType, ShiftAmount,
                          Preindexed, Postindexed, Negative, Writeback, S, E);
    return false;
  }

  return true;
}

/// Parse the offset of a memory operand after we have seen "[Rn," or "[Rn],"
/// we will parse the following (were +/- means that a plus or minus is
/// optional):
///   +/-Rm
///   +/-Rm, shift
///   #offset
/// we return false on success or an error otherwise.
bool ARMAsmParser::ParseMemoryOffsetReg(bool &Negative,
                                        bool &OffsetRegShifted,
                                        enum ShiftType &ShiftType,
                                        const MCExpr *&ShiftAmount,
                                        const MCExpr *&Offset,
                                        bool &OffsetIsReg,
                                        int &OffsetRegNum,
                                        SMLoc &E) {
  OwningPtr<ARMOperand> Op;
  Negative = false;
  OffsetRegShifted = false;
  OffsetIsReg = false;
  OffsetRegNum = -1;
  const AsmToken &NextTok = getTok();
  E = NextTok.getLoc();
  if (NextTok.is(AsmToken::Plus))
    Lex(); // Eat plus token.
  else if (NextTok.is(AsmToken::Minus)) {
    Negative = true;
    Lex(); // Eat minus token
  }
  // See if there is a register following the "[Rn," or "[Rn]," we have so far.
  const AsmToken &OffsetRegTok = getTok();
  if (OffsetRegTok.is(AsmToken::Identifier)) {
    OffsetIsReg = !MaybeParseRegister(Op, false);
    if (OffsetIsReg) {
      E = Op->getEndLoc();
      OffsetRegNum = Op->getReg();
    }
  }
  // If we parsed a register as the offset then their can be a shift after that
  if (OffsetRegNum != -1) {
    // Look for a comma then a shift
    const AsmToken &Tok = getTok();
    if (Tok.is(AsmToken::Comma)) {
      Lex(); // Eat comma token.

      const AsmToken &Tok = getTok();
      if (ParseShift(ShiftType, ShiftAmount, E))
	return Error(Tok.getLoc(), "shift expected");
      OffsetRegShifted = true;
    }
  }
  else { // the "[Rn," or "[Rn,]" we have so far was not followed by "Rm"
    // Look for #offset following the "[Rn," or "[Rn],"
    const AsmToken &HashTok = getTok();
    if (HashTok.isNot(AsmToken::Hash))
      return Error(HashTok.getLoc(), "'#' expected");
    
    Lex(); // Eat hash token.

    if (ParseExpression(Offset))
     return true;
    E = SMLoc::getFromPointer(getTok().getLoc().getPointer() - 1);
  }
  return false;
}

/// ParseShift as one of these two:
///   ( lsl | lsr | asr | ror ) , # shift_amount
///   rrx
/// and returns true if it parses a shift otherwise it returns false.
bool ARMAsmParser::ParseShift(ShiftType &St, 
                              const MCExpr *&ShiftAmount, 
                              SMLoc &E) {
  const AsmToken &Tok = getTok();
  if (Tok.isNot(AsmToken::Identifier))
    return true;
  const StringRef &ShiftName = Tok.getString();
  if (ShiftName == "lsl" || ShiftName == "LSL")
    St = Lsl;
  else if (ShiftName == "lsr" || ShiftName == "LSR")
    St = Lsr;
  else if (ShiftName == "asr" || ShiftName == "ASR")
    St = Asr;
  else if (ShiftName == "ror" || ShiftName == "ROR")
    St = Ror;
  else if (ShiftName == "rrx" || ShiftName == "RRX")
    St = Rrx;
  else
    return true;
  Lex(); // Eat shift type token.

  // Rrx stands alone.
  if (St == Rrx)
    return false;

  // Otherwise, there must be a '#' and a shift amount.
  const AsmToken &HashTok = getTok();
  if (HashTok.isNot(AsmToken::Hash))
    return Error(HashTok.getLoc(), "'#' expected");
  Lex(); // Eat hash token.

  if (ParseExpression(ShiftAmount))
    return true;

  return false;
}

/// A hack to allow some testing, to be replaced by a real table gen version.
int ARMAsmParser::MatchRegisterName(const StringRef &Name) {
  if (Name == "r0" || Name == "R0")
    return 0;
  else if (Name == "r1" || Name == "R1")
    return 1;
  else if (Name == "r2" || Name == "R2")
    return 2;
  else if (Name == "r3" || Name == "R3")
    return 3;
  else if (Name == "r3" || Name == "R3")
    return 3;
  else if (Name == "r4" || Name == "R4")
    return 4;
  else if (Name == "r5" || Name == "R5")
    return 5;
  else if (Name == "r6" || Name == "R6")
    return 6;
  else if (Name == "r7" || Name == "R7")
    return 7;
  else if (Name == "r8" || Name == "R8")
    return 8;
  else if (Name == "r9" || Name == "R9")
    return 9;
  else if (Name == "r10" || Name == "R10")
    return 10;
  else if (Name == "r11" || Name == "R11" || Name == "fp")
    return 11;
  else if (Name == "r12" || Name == "R12" || Name == "ip")
    return 12;
  else if (Name == "r13" || Name == "R13" || Name == "sp")
    return 13;
  else if (Name == "r14" || Name == "R14" || Name == "lr")
      return 14;
  else if (Name == "r15" || Name == "R15" || Name == "pc")
    return 15;
  return -1;
}

/// A hack to allow some testing, to be replaced by a real table gen version.
bool ARMAsmParser::
MatchInstruction(const SmallVectorImpl<MCParsedAsmOperand*> &Operands,
                 MCInst &Inst) {
  ARMOperand &Op0 = *(ARMOperand*)Operands[0];
  assert(Op0.Kind == ARMOperand::Token && "First operand not a Token");
  const StringRef &Mnemonic = Op0.getToken();
  if (Mnemonic == "add" ||
      Mnemonic == "stmfd" ||
      Mnemonic == "str" ||
      Mnemonic == "ldmfd" ||
      Mnemonic == "ldr" ||
      Mnemonic == "mov" ||
      Mnemonic == "sub" ||
      Mnemonic == "bl" ||
      Mnemonic == "push" ||
      Mnemonic == "blx" ||
      Mnemonic == "pop") {
    // Hard-coded to a valid instruction, till we have a real matcher.
    Inst = MCInst();
    Inst.setOpcode(ARM::MOVr);
    Inst.addOperand(MCOperand::CreateReg(2));
    Inst.addOperand(MCOperand::CreateReg(2));
    Inst.addOperand(MCOperand::CreateImm(0));
    Inst.addOperand(MCOperand::CreateImm(0));
    Inst.addOperand(MCOperand::CreateReg(0));
    return false;
  }

  return true;
}

/// Parse a arm instruction operand.  For now this parses the operand regardless
/// of the mnemonic.
bool ARMAsmParser::ParseOperand(OwningPtr<ARMOperand> &Op) {
  SMLoc S, E;
  
  switch (getLexer().getKind()) {
  case AsmToken::Identifier:
    if (!MaybeParseRegister(Op, true))
      return false;
    // This was not a register so parse other operands that start with an
    // identifier (like labels) as expressions and create them as immediates.
    const MCExpr *IdVal;
    S = getTok().getLoc();
    if (ParseExpression(IdVal))
      return true;
    E = SMLoc::getFromPointer(getTok().getLoc().getPointer() - 1);
    ARMOperand::CreateImm(Op, IdVal, S, E);
    return false;
  case AsmToken::LBrac:
    return ParseMemory(Op);
  case AsmToken::LCurly:
    return ParseRegisterList(Op);
  case AsmToken::Hash:
    // #42 -> immediate.
    // TODO: ":lower16:" and ":upper16:" modifiers after # before immediate
    S = getTok().getLoc();
    Lex();
    const MCExpr *ImmVal;
    if (ParseExpression(ImmVal))
      return true;
    E = SMLoc::getFromPointer(getTok().getLoc().getPointer() - 1);
    ARMOperand::CreateImm(Op, ImmVal, S, E);
    return false;
  default:
    return Error(getTok().getLoc(), "unexpected token in operand");
  }
}

/// Parse an arm instruction mnemonic followed by its operands.
bool ARMAsmParser::ParseInstruction(const StringRef &Name, SMLoc NameLoc,
                               SmallVectorImpl<MCParsedAsmOperand*> &Operands) {
  OwningPtr<ARMOperand> Op;
  ARMOperand::CreateToken(Op, Name, NameLoc);
  
  Operands.push_back(Op.take());

  SMLoc Loc = getTok().getLoc();
  if (getLexer().isNot(AsmToken::EndOfStatement)) {

    // Read the first operand.
    OwningPtr<ARMOperand> Op;
    if (ParseOperand(Op)) return true;
    Operands.push_back(Op.take());

    while (getLexer().is(AsmToken::Comma)) {
      Lex();  // Eat the comma.

      // Parse and remember the operand.
      if (ParseOperand(Op)) return true;
      Operands.push_back(Op.take());
    }
  }
  return false;
}

/// ParseTargetDirective parses the arm specific directives
bool ARMAsmParser::ParseTargetDirective(AsmToken DirectiveID) {
  StringRef IDVal = DirectiveID.getIdentifier();
  if (IDVal == ".word")
    return ParseDirectiveWord(4, DirectiveID.getLoc());
  else if (IDVal == ".thumb")
    return ParseDirectiveThumb(DirectiveID.getLoc());
  else if (IDVal == ".thumb_func")
    return ParseDirectiveThumbFunc(DirectiveID.getLoc());
  else if (IDVal == ".code")
    return ParseDirectiveCode(DirectiveID.getLoc());
  else if (IDVal == ".syntax")
    return ParseDirectiveSyntax(DirectiveID.getLoc());
  return true;
}

/// ParseDirectiveWord
///  ::= .word [ expression (, expression)* ]
bool ARMAsmParser::ParseDirectiveWord(unsigned Size, SMLoc L) {
  if (getLexer().isNot(AsmToken::EndOfStatement)) {
    for (;;) {
      const MCExpr *Value;
      if (ParseExpression(Value))
        return true;

      getStreamer().EmitValue(Value, Size, 0/*addrspace*/);

      if (getLexer().is(AsmToken::EndOfStatement))
        break;
      
      // FIXME: Improve diagnostic.
      if (getLexer().isNot(AsmToken::Comma))
        return Error(L, "unexpected token in directive");
      Lex();
    }
  }

  Lex();
  return false;
}

/// ParseDirectiveThumb
///  ::= .thumb
bool ARMAsmParser::ParseDirectiveThumb(SMLoc L) {
  if (getLexer().isNot(AsmToken::EndOfStatement))
    return Error(L, "unexpected token in directive");
  Lex();

  // TODO: set thumb mode
  // TODO: tell the MC streamer the mode
  // getStreamer().Emit???();
  return false;
}

/// ParseDirectiveThumbFunc
///  ::= .thumbfunc symbol_name
bool ARMAsmParser::ParseDirectiveThumbFunc(SMLoc L) {
  const AsmToken &Tok = getTok();
  if (Tok.isNot(AsmToken::Identifier) && Tok.isNot(AsmToken::String))
    return Error(L, "unexpected token in .syntax directive");
  StringRef ATTRIBUTE_UNUSED SymbolName = getTok().getIdentifier();
  Lex(); // Consume the identifier token.

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return Error(L, "unexpected token in directive");
  Lex();

  // TODO: mark symbol as a thumb symbol
  // getStreamer().Emit???();
  return false;
}

/// ParseDirectiveSyntax
///  ::= .syntax unified | divided
bool ARMAsmParser::ParseDirectiveSyntax(SMLoc L) {
  const AsmToken &Tok = getTok();
  if (Tok.isNot(AsmToken::Identifier))
    return Error(L, "unexpected token in .syntax directive");
  const StringRef &Mode = Tok.getString();
  bool unified_syntax;
  if (Mode == "unified" || Mode == "UNIFIED") {
    Lex();
    unified_syntax = true;
  }
  else if (Mode == "divided" || Mode == "DIVIDED") {
    Lex();
    unified_syntax = false;
  }
  else
    return Error(L, "unrecognized syntax mode in .syntax directive");

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return Error(getTok().getLoc(), "unexpected token in directive");
  Lex();

  // TODO tell the MC streamer the mode
  // getStreamer().Emit???();
  return false;
}

/// ParseDirectiveCode
///  ::= .code 16 | 32
bool ARMAsmParser::ParseDirectiveCode(SMLoc L) {
  const AsmToken &Tok = getTok();
  if (Tok.isNot(AsmToken::Integer))
    return Error(L, "unexpected token in .code directive");
  int64_t Val = getTok().getIntVal();
  bool thumb_mode;
  if (Val == 16) {
    Lex();
    thumb_mode = true;
  }
  else if (Val == 32) {
    Lex();
    thumb_mode = false;
  }
  else
    return Error(L, "invalid operand to .code directive");

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return Error(getTok().getLoc(), "unexpected token in directive");
  Lex();

  // TODO tell the MC streamer the mode
  // getStreamer().Emit???();
  return false;
}

extern "C" void LLVMInitializeARMAsmLexer();

/// Force static initialization.
extern "C" void LLVMInitializeARMAsmParser() {
  RegisterAsmParser<ARMAsmParser> X(TheARMTarget);
  RegisterAsmParser<ARMAsmParser> Y(TheThumbTarget);
  LLVMInitializeARMAsmLexer();
}
