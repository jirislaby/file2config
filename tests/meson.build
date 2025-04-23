# SPDX-License-Identifier: GPL-2.0-only

test_parser = executable('test_parser', [
    'test_parser.cpp',
    '../src/Verbose.cpp',
  ],
  cpp_args: '-DTESTS_DIR="' + meson.current_source_dir() + '"',
  link_with: parser,
  include_directories: include_directories('../src/parser'),
)

test('parser unit tests', test_parser)

