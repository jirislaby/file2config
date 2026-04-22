# SPDX-License-Identifier: GPL-2.0-only

project('file2config', 'cpp',
  version: '1',
  default_options: [
    'optimization=g',
    'warning_level=2',
    'b_lto=true',
    'b_pie=true',
    'cpp_std=c++20',
  ])

add_project_arguments('-ggdb', language : 'cpp')
cpp_compiler = meson.get_compiler('cpp')

cxxopts_dep = dependency('cxxopts')
json_dep = dependency('nlohmann_json')
slcurl_dep = dependency('slcurl++')
slgit_dep = dependency('slgit++')
slhelpers_dep = dependency('slhelpers++')
slkerncvs_dep = dependency('slkerncvs++')
slsqlite_dep = dependency('slsqlite++')

antlr4 = find_program('antlr4')
antlr4_cmd = [ antlr4, '-Xexact-output-dir', '-o', '@OUTDIR@', '-Dlanguage=Cpp',
  '-message-format', 'gnu', '@INPUT@' ]
antlr4_noinc_dep = dependency('antlr4-runtime', modules: ['antlr4_shared'])
antlr4_dep = declare_dependency(
  dependencies: antlr4_noinc_dep,
  include_directories: antlr4_noinc_dep.get_variable(cmake : 'ANTLR4_INCLUDE_DIR'),
)
antlr4_out_files = [
  '@BASENAME@.cpp',
  '@BASENAME@.h',
  '@BASENAME@.interp',
  '@BASENAME@.tokens',
]
antlr4_listener_out_files = [
  '@BASENAME@BaseListener.cpp',
  '@BASENAME@BaseListener.h',
  '@BASENAME@Listener.cpp',
  '@BASENAME@Listener.h',
]
antlr4_warnings = [ '-Wno-overloaded-virtual', '-Wno-unused-parameter' ]

subdir('f2c_cli')
subdir('f2c_create_db')
subdir('tests')

install_subdir('data',
  install_dir : get_option('datadir') / meson.project_name(),
  strip_directory : true
)
install_subdir('scripts', install_dir : 'bin', strip_directory : true)
