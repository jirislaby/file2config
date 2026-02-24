# SPDX-License-Identifier: GPL-2.0-only

antlr4_noinc = dependency('antlr4-runtime', modules: ['antlr4_shared'])
antlr4_lib = declare_dependency(
  dependencies: antlr4_noinc,
  include_directories: antlr4_noinc.get_variable(cmake : 'ANTLR4_INCLUDE_DIR'),
)

antlr4 = find_program('antlr4')

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

parser = static_library('parser', [
    'Parser.cpp',
    'Parser.h',
    'EntryVisitor.h',
    'ErrorListener.cpp',
    'ErrorListener.h',
    'MakeExprListener.cpp',
    'MakeExprListener.h',
    gen_antlr,
  ],
  # the generated headers are buggy
  cpp_args: [ '-Wno-overloaded-virtual' ],
  dependencies : antlr4_lib,
)
