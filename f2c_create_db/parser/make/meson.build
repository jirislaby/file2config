# SPDX-License-Identifier: GPL-2.0-only

lexer_copy = custom_target('lexer_copy',
  input : 'MakeLexer.g4',
  output : 'MakeLexer.g4',
  command : [ 'cp', '@INPUT@', '@OUTPUT@' ]
)

lexer_gen_antlr = custom_target('lexer_gen_antlr',
  input : lexer_copy,
  output : [
    'MakeLexer.cpp',
    'MakeLexer.h',
  ],
  command : [ antlr4, '-Dlanguage=Cpp', '-message-format', 'gnu', '@INPUT@' ],
)

# Parser

parser_copy = custom_target('parser_copy',
  input : 'MakeParser.g4',
  output : 'MakeParser.g4',
  command : [ 'cp', '@INPUT@', '@OUTPUT@' ]
)

parser_gen_antlr = custom_target('parser_gen_antlr',
  depends : lexer_gen_antlr,
  input : parser_copy,
  output : [
    'MakeParser.cpp',
    'MakeParser.h',
    'MakeParserListener.cpp',
    'MakeParserListener.h',
  ],
  command : [ antlr4, '-Dlanguage=Cpp', '-message-format', 'gnu', '@INPUT@' ],
)

# Resulting lib

make_parser_lib = static_library('make_parser', [
    'Parser.cpp',
    'Parser.h',
    'MakeParserExprListener.cpp',
    'MakeParserExprListener.h',
    lexer_gen_antlr,
    parser_gen_antlr,
  ],
  # the generated headers are buggy
  cpp_args: [ '-Wno-overloaded-virtual', '-Wno-unused-parameter' ],
  dependencies : antlr4_dep,
)

make_parser = declare_dependency(
  link_with: make_parser_lib,
  sources: [ lexer_gen_antlr[1], parser_gen_antlr[1] ],
)
