##===- examples/OCaml-Kaleidoscope/Chapter7/Makefile -------*- Makefile -*-===##
# 
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
# 
##===----------------------------------------------------------------------===##
# 
# This is the makefile for the Objective Caml kaleidoscope tutorial, chapter 7.
# 
##===----------------------------------------------------------------------===##

LEVEL := ../../..
TOOLNAME := OCaml-Kaleidoscope-Ch7
EXAMPLE_TOOL := 1
UsedComponents := core
UsedOcamLibs := llvm llvm_analysis llvm_executionengine llvm_target \
	llvm_scalar_opts

OCAMLCFLAGS += -pp camlp4of

ExcludeSources = $(PROJ_SRC_DIR)/myocamlbuild.ml

include $(LEVEL)/bindings/ocaml/Makefile.ocaml
