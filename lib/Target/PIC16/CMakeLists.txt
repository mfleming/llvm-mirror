set(LLVM_TARGET_DEFINITIONS PIC16.td)

tablegen(PIC16GenRegisterInfo.h.inc -gen-register-desc-header)
tablegen(PIC16GenRegisterNames.inc -gen-register-enums)
tablegen(PIC16GenRegisterInfo.inc -gen-register-desc)
tablegen(PIC16GenInstrNames.inc -gen-instr-enums)
tablegen(PIC16GenInstrInfo.inc -gen-instr-desc)
tablegen(PIC16GenAsmWriter.inc -gen-asm-writer)
tablegen(PIC16GenDAGISel.inc -gen-dag-isel)
tablegen(PIC16GenCallingConv.inc -gen-callingconv)
tablegen(PIC16GenSubtarget.inc -gen-subtarget)

add_llvm_target(PIC16
  PIC16DebugInfo.cpp
  PIC16InstrInfo.cpp
  PIC16ISelDAGToDAG.cpp
  PIC16ISelLowering.cpp
  PIC16MemSelOpt.cpp
  PIC16MCAsmInfo.cpp
  PIC16RegisterInfo.cpp
  PIC16Section.cpp
  PIC16Subtarget.cpp
  PIC16TargetMachine.cpp
  PIC16TargetObjectFile.cpp
  PIC16SelectionDAGInfo.cpp
  )
