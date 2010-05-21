; RUN: llc < %s -march=thumb | FileCheck %s
; rdar://7961298

define arm_apcscc void @t() nounwind {
entry:
; CHECK: t:
; CHECK: trap
  call void @llvm.trap()
  unreachable
}

declare void @llvm.trap() nounwind