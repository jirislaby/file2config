# SPDX-License-Identifier: GPL-2.0-only

cxxopts_lib = dependency('cxxopts')
json = dependency('nlohmann_json')
slcurl_lib = dependency('slcurl++')
slgit_lib = dependency('slgit++')
slhelpers_lib = dependency('slhelpers++')
slsqlite_lib = dependency('slsqlite++')

executable('f2c_cli', [
    'main.cpp',
  ],
  dependencies: [ cxxopts_lib, json, slcurl_lib, slgit_lib, slhelpers_lib, slsqlite_lib],
  install: true,
)
