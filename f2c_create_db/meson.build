# SPDX-License-Identifier: GPL-2.0-only

subdir('parser')
subdir('sql')
subdir('treewalker')

# for qt-creator to pick up generated headers
builddir = include_directories('.')

executable('f2c_create_db', [
    'main.cpp',
    'BranchProps.cpp',
    'BranchProps.h',
    'BranchProcessor.cpp',
    'BranchProcessor.h',
    'Ignores.cpp',
    'Ignores.h',
    'Opts.cpp',
    'Opts.h',
    'Renames.cpp',
    'Renames.h',
    'StatusNotifier.h',
    'Verbose.cpp',
    'Verbose.h',
  ],
  link_with: [ sqlite, treewalker ],
  dependencies: [ cxxopts_dep, json_dep, slgit_dep, slhelpers_dep, slkerncvs_dep ],
  install: true,
)
