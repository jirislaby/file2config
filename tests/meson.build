# SPDX-License-Identifier: GPL-2.0-only

test_parser = executable('test_parser', [
    'test_parser.cpp',
    '../f2c_create_db/Verbose.cpp',
  ],
  cpp_args: '-DTESTS_DIR="' + meson.current_source_dir() + '"',
  link_with: parsers,
  include_directories: include_directories('../f2c_create_db/parser'),
)

test('parser unit tests', test_parser)

