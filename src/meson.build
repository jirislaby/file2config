# SPDX-License-Identifier: GPL-2.0-only

subdir('parser')
subdir('sql')
subdir('treewalker')

cpp_compiler.has_header('cxxopts.hpp',
  required: true,
)
cpp_compiler.has_header('sl/helpers/PushD.h',
  required: true,
)

slkerncvs_lib = cpp_compiler.find_library('slkerncvs++')
slgit_lib = cpp_compiler.find_library('slgit++')
git_lib = cpp_compiler.find_library('git2')

# for qt-creator to pick up generated headers
builddir = include_directories('.')

executable('f2c_create_db', [
    'main.cpp',
    'Verbose.cpp',
    'Verbose.h',
  ],
  link_with: [ sqlite, treewalker ],
  dependencies: [ slkerncvs_lib, slgit_lib, git_lib ],
  install: true,
)
