# SPDX-License-Identifier: GPL-2.0-only

subdir('parser')
subdir('sql')
subdir('treewalker')

# for qt-creator to pick up generated headers
builddir = include_directories('.')

executable('f2c_collect_configs', [
    'main.cpp',
    'Verbose.cpp',
    'Verbose.h',
  ],
  link_with: [ sqlite, treewalker ],
  install: true,
)
