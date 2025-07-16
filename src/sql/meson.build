# SPDX-License-Identifier: GPL-2.0-only

sqlite3_lib = dependency('slsqlite++')

sqlite = static_library('sqlite', [
    'F2CSQLConn.cpp',
    'F2CSQLConn.h',
  ],
  dependencies: sqlite3_lib,
)
