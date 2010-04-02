//===---- llvm/Support/DebugLoc.h - Debug Location Information --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines a number of light weight data structures used
// to describe and track debug location information.
// 
//===----------------------------------------------------------------------===//

#ifndef LLVM_DEBUGLOC_H
#define LLVM_DEBUGLOC_H

#include "llvm/ADT/DenseMap.h"
#include <vector>

namespace llvm {
  class MDNode;
  class LLVMContext;
  
  /// DebugLoc - Debug location id.  This is carried by Instruction, SDNode,
  /// and MachineInstr to compactly encode file/line/scope information for an
  /// operation.
  class NewDebugLoc {
    /// LineCol - This 32-bit value encodes the line and column number for the
    /// location, encoded as 24-bits for line and 8 bits for col.  A value of 0
    /// for either means unknown.
    unsigned LineCol;
    
    /// ScopeIdx - This is an opaque ID# for Scope/InlinedAt information,
    /// decoded by LLVMContext.  0 is unknown.
    int ScopeIdx;
  public:
    NewDebugLoc() : LineCol(0), ScopeIdx(0) {}  // Defaults to unknown.
    
    /// get - Get a new DebugLoc that corresponds to the specified line/col
    /// scope/inline location.
    static NewDebugLoc get(unsigned Line, unsigned Col,
                           MDNode *Scope, MDNode *InlinedAt = 0);
    
    /// getFromDILocation - Translate the DILocation quad into a NewDebugLoc.
    static NewDebugLoc getFromDILocation(MDNode *N);
    
    /// isUnknown - Return true if this is an unknown location.
    bool isUnknown() const { return ScopeIdx == 0; }
    
    unsigned getLine() const {
      return (LineCol << 8) >> 8;  // Mask out column.
    }
    
    unsigned getCol() const {
      return LineCol >> 24;
    }
    
    /// getScope - This returns the scope pointer for this DebugLoc, or null if
    /// invalid.
    MDNode *getScope(const LLVMContext &Ctx) const;
    
    /// getInlinedAt - This returns the InlinedAt pointer for this DebugLoc, or
    /// null if invalid or not present.
    MDNode *getInlinedAt(const LLVMContext &Ctx) const;
    
    /// getScopeAndInlinedAt - Return both the Scope and the InlinedAt values.
    void getScopeAndInlinedAt(MDNode *&Scope, MDNode *&IA,
                              const LLVMContext &Ctx) const;
    
    
    /// getAsMDNode - This method converts the compressed DebugLoc node into a
    /// DILocation compatible MDNode.
    MDNode *getAsMDNode(const LLVMContext &Ctx) const;
    
    bool operator==(const NewDebugLoc &DL) const {
      return LineCol == DL.LineCol && ScopeIdx == DL.ScopeIdx;
    }
    bool operator!=(const NewDebugLoc &DL) const { return !(*this == DL); }
    
    
    
    
    static NewDebugLoc getUnknownLoc() { NewDebugLoc L; return L; }
  };
  
  
  typedef NewDebugLoc DebugLoc;

} // end namespace llvm

#endif /* LLVM_DEBUGLOC_H */
