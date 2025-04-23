# SPDX-License-Identifier: GPL-2.0-only

sqlite3_lib = cpp_compiler.find_library('sqlite3')

sqlite = static_library('sqlite', [
    'SQLConn.cpp',
    'SQLConn.h',
    'SQLiteSmart.h',
  ],
  dependencies: sqlite3_lib,
)
