# SPDX-License-Identifier: GPL-2.0-only

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
  dependencies: [ antlr4_dep, kconfig_parser, make_parser ],
)
