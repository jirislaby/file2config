# SPDX-License-Identifier: GPL-2.0-only

copy_grammar = custom_target('copy_grammar',
  input : 'Make.g4',
  output : 'Make.g4',
  command : [ 'cp', '@INPUT@', '@OUTPUT@' ]
)

gen_antlr = custom_target('gen_antlr',
  input : copy_grammar,
  output : [
    'MakeLexer.cpp',
    'MakeLexer.h',
    'MakeParser.cpp',
    'MakeParser.h',
    'MakeListener.cpp',
    'MakeListener.h',
  ],
  command : [ antlr4, '-Dlanguage=Cpp', '-message-format', 'gnu', '@INPUT@' ],
)

make_parser = static_library('make_parser', [
    'Parser.cpp',
    'Parser.h',
    'EntryVisitor.h',
    'MakeExprListener.cpp',
    'MakeExprListener.h',
    gen_antlr,
  ],
  # antlr headers are buggy
  cpp_args: [ '-Wno-overloaded-virtual' ],
  dependencies : antlr4_lib,
)
