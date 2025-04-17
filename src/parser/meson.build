# SPDX-License-Identifier: GPL-2.0-only

generated_files = [
  'MakeLexer.cpp',
  'MakeLexer.h',
  'MakeParser.cpp',
  'MakeParser.h',
  'MakeListener.cpp',
  'MakeListener.h',
]

# the generated headers are buggy
add_project_arguments('-Wno-overloaded-virtual', language : 'cpp')

parser_include = include_directories('/usr/include/antlr4-runtime/')
antlr4_lib = cpp_compiler.find_library('antlr4-runtime',
  has_headers : 'antlr4-runtime.h',
  header_include_directories : parser_include
)

antlr4 = find_program('antlr4')

copy_grammar = custom_target('copy_grammar',
  input : 'Make.g4',
  output : 'Make.g4',
  command : [ 'cp', '@INPUT@', '@OUTPUT@' ]
)

gen_antlr = custom_target('gen_antlr',
  input : copy_grammar,
  output : generated_files,
  command : [ antlr4, '-Dlanguage=Cpp', '@INPUT@' ],
)

parser_generated_files = declare_dependency(
  sources : gen_antlr
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
  dependencies : antlr4_lib,
  include_directories : parser_include,
)
