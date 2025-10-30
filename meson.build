# SPDX-License-Identifier: GPL-2.0-only

project('file2config', 'cpp',
  version: '1',
  default_options: [
    'optimization=g',
    'warning_level=2',
    'b_lto=true',
    'b_pie=true',
    'cpp_std=c++17',
  ])

add_project_arguments('-ggdb', language : 'cpp')
cpp_compiler = meson.get_compiler('cpp')

subdir('src')
subdir('tests')

install_subdir('data',
  install_dir : get_option('datadir') / meson.project_name(),
  strip_directory : true
)
install_subdir('scripts', install_dir : 'bin', strip_directory : true)
