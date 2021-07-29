# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

# (rm -rf build; meson build; cd build; time ninja -v)

project(
  'atomic_queue', 'cpp',
  license : 'MIT License',
  default_options : ['cpp_std=gnu++14', 'buildtype=release', 'b_ndebug=if-release']
)

cxx = meson.get_compiler('cpp')
dl = cxx.find_library('dl', required : true)
threads = dependency('threads')
unit_test_framework = dependency('boost', modules : ['unit_test_framework'])
if get_option('benchmarks')
  tbb = cxx.find_library('tbb', required : true)
  xenium = declare_dependency(include_directories : '../xenium')
  moodycamel = declare_dependency(include_directories : '../')
endif

atomic_queue = declare_dependency(include_directories : ['include'], dependencies : threads)

tests_exe = executable(
  'tests',
  'src/tests.cc',
  dependencies : [atomic_queue, unit_test_framework]
)
test('tests', tests_exe)

example_exe = executable(
  'example',
  'src/example.cc',
  dependencies : [atomic_queue]
)

if get_option('benchmarks')
  benchmarks_exe = executable(
    'benchmarks',
    ['src/benchmarks.cc', 'src/cpu_base_frequency.cc', 'src/huge_pages.cc'],
    dependencies : [atomic_queue, xenium, moodycamel, tbb, dl]
  )
endif
