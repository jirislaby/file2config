# SPDX-License-Identifier: GPL-2.0-only

sqlite3_lib = cpp_compiler.find_library('slsqlite++')

sqlite = static_library('sqlite', [
    'F2CSQLConn.cpp',
    'F2CSQLConn.h',
  ],
  dependencies: sqlite3_lib,
)
