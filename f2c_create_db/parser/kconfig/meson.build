# SPDX-License-Identifier: GPL-2.0-only

# Lexer

lexer_copy = custom_target('lexer_copy',
  input : 'KconfigLexer.g4',
  output : 'KconfigLexer.g4',
  command : [ 'cp', '@INPUT@', '@OUTPUT@' ]
)

lexer_gen_antlr = custom_target('lexer_gen_antlr',
  input : lexer_copy,
  output : [
    'KconfigLexer.cpp',
    'KconfigLexer.h',
  ],
  command : [ antlr4, '-Dlanguage=Cpp', '-message-format', 'gnu', '@INPUT@' ],
)

# Parser

parser_copy = custom_target('parser_copy',
  input : 'KconfigParser.g4',
  output : 'KconfigParser.g4',
  command : [ 'cp', '@INPUT@', '@OUTPUT@' ]
)

parser_gen_antlr = custom_target('parser_gen_antlr',
  depends : lexer_gen_antlr,
  input : parser_copy,
  output : [
    'KconfigParser.cpp',
    'KconfigParser.h',
    'KconfigParserListener.cpp',
    'KconfigParserListener.h',
  ],
  command : [ antlr4, '-Dlanguage=Cpp', '-message-format', 'gnu', '@INPUT@' ],
)

# Resulting lib

kconfig_parser_lib = static_library('kconfig_parser', [
    'Parser.cpp',
    'Parser.h',
    'KconfigParserConfigListener.cpp',
    'KconfigParserConfigListener.h',
    lexer_gen_antlr,
    parser_gen_antlr,
  ],
  # the generated headers are buggy
  cpp_args: [ '-Wno-overloaded-virtual', '-Wno-unused-parameter' ],
  dependencies : antlr4_lib,
)

kconfig_parser = declare_dependency(
  link_with: kconfig_parser_lib,
  sources: [ lexer_gen_antlr[1], parser_gen_antlr[1] ],
)
