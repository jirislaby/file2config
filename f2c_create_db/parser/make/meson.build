# SPDX-License-Identifier: GPL-2.0-only

lexer_gen_antlr = custom_target('lexer_gen_antlr',
  input : 'MakeLexer.g4',
  output : antlr4_out_files,
  command : antlr4_cmd,
)

parser_gen_antlr = custom_target('parser_gen_antlr',
  depends : lexer_gen_antlr,
  input : 'MakeParser.g4',
  output : antlr4_out_files + antlr4_listener_out_files,
  command : antlr4_cmd,
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
  cpp_args: antlr4_warnings,
  dependencies : antlr4_dep,
)

make_parser = declare_dependency(
  link_with: make_parser_lib,
  sources: [ lexer_gen_antlr[1], parser_gen_antlr[1] ],
)
