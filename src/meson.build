# SPDX-License-Identifier: GPL-2.0-only

subdir('parser')
subdir('sql')
subdir('treewalker')

cxxopts_lib = dependency('cxxopts')
slgit_lib = dependency('slgit++')
slhelpers_lib = dependency('slhelpers++')
slkerncvs_lib = dependency('slkerncvs++')

# for qt-creator to pick up generated headers
builddir = include_directories('.')

executable('f2c_create_db', [
    'main.cpp',
    'Verbose.cpp',
    'Verbose.h',
  ],
  link_with: [ sqlite, treewalker ],
  dependencies: [ cxxopts_lib, slgit_lib, slhelpers_lib, slkerncvs_lib ],
  install: true,
)
