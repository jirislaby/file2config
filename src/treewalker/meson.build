# SPDX-License-Identifier: GPL-2.0-only

treewalker = static_library('treewalker', [
    'treewalker.cpp',
    'treewalker.h',
  ],
  link_with: [ parser ],
  dependencies: [ parser_generated_files ],
  include_directories : [ parser_include ],
)
