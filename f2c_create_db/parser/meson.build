# SPDX-License-Identifier: GPL-2.0-only

antlr4_noinc = dependency('antlr4-runtime', modules: ['antlr4_shared'])
antlr4_lib = declare_dependency(
  dependencies: antlr4_noinc,
  include_directories: antlr4_noinc.get_variable(cmake : 'ANTLR4_INCLUDE_DIR'),
)

antlr4 = find_program('antlr4')

subdir('make')

parsers = static_library('parsers', [
    'ErrorListener.cpp',
    'ErrorListener.h',
  ],
  # antlr headers are buggy
  cpp_args: [ '-Wno-overloaded-virtual' ],
  link_with: [ make_parser ],
  dependencies: antlr4_lib,
)
