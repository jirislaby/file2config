# SPDX-License-Identifier: GPL-2.0-only

antlr4_noinc = dependency('antlr4-runtime', modules: ['antlr4_shared'])
antlr4_lib = declare_dependency(
  dependencies: antlr4_noinc,
  include_directories: antlr4_noinc.get_variable(cmake : 'ANTLR4_INCLUDE_DIR'),
)

antlr4 = find_program('antlr4')

subdir('kconfig')
subdir('make')

parsers = static_library('parsers', [
    'ErrorListener.cpp',
    'ErrorListener.h',
    'Parser.cpp',
    'Parser.h',
  ],
  # antlr headers are buggy
  cpp_args: [ '-Wno-overloaded-virtual' ],
  dependencies: [ antlr4_lib, kconfig_parser, make_parser ],
)
