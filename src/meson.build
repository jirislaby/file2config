subdir('parser')
subdir('treewalker')

# for qt-creator to pick up generated headers
builddir = include_directories('.')

executable('f2c_collect_configs', [
    'main.cpp'
  ],
  link_with: [ treewalker ],
  dependencies: [ parser_generated_files ],
  include_directories : [ builddir, parser_include ],
  install: true,
)
