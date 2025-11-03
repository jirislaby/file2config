# SPDX-License-Identifier: GPL-2.0-only

cxxopts_lib = dependency('cxxopts')
slcurl_lib = dependency('slcurl++')
slgit_lib = dependency('slgit++')
slhelpers_lib = dependency('slhelpers++')
slsqlite_lib = dependency('slsqlite++')

executable('f2c_cli', [
    'main.cpp',
  ],
  dependencies: [ cxxopts_lib, slcurl_lib, slgit_lib, slhelpers_lib, slsqlite_lib],
  install: true,
)
