# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

# (rm -rf build; meson setup build; time ninja -C build -v)

project(
  'atomic_queue', 'cpp',
  license : 'MIT License',
  default_options : ['cpp_std=gnu++14', 'buildtype=release', 'b_ndebug=if-release']
)

threads_dep = dependency('threads')

atomic_queue_dep = declare_dependency(include_directories : ['include'], dependencies : threads_dep)

if get_option('tests')
  unit_test_framework_dep = dependency('boost', modules : ['unit_test_framework'])

  tests_exe = executable(
    'tests',
    'src/tests.cc',
    dependencies : [atomic_queue_dep, unit_test_framework_dep],
  )
  test('tests', tests_exe)

  example_exe = executable(
    'example',
    'src/example.cc',
    dependencies : [atomic_queue_dep],
  )
  test('example', example_exe)
endif

if get_option('benchmarks')
  incompatible_cpp_std = ['c++98', 'c++03', 'c++11', 'c++14', 'gnu++03', 'gnu++11', 'gnu++14', 'vc++14']
  if incompatible_cpp_std.contains(get_option('cpp_std'))
    error('Benchmarks require C++17 or higher')
  endif

  cxx = meson.get_compiler('cpp')
  dl_dep = cxx.find_library('dl')
  xenium_dep = declare_dependency(include_directories : '../xenium')
  boost_dep = dependency('boost')
  tbb_dep = cxx.find_library('tbb')
  moodycamel_dep = declare_dependency(include_directories : '../')

  executable(
    'benchmarks',
    ['src/benchmarks.cc', 'src/cpu_base_frequency.cc', 'src/huge_pages.cc'],
    include_directories : ['src'],
    dependencies : [atomic_queue_dep, dl_dep, xenium_dep, boost_dep, tbb_dep, moodycamel_dep],
  )
endif

install_headers(
  files(
    'include/atomic_queue/atomic_queue.h',
    'include/atomic_queue/atomic_queue_mutex.h',
    'include/atomic_queue/barrier.h',
    'include/atomic_queue/defs.h',
    'include/atomic_queue/spinlock.h',
  ),
  subdir: 'atomic_queue'
)
pkg = import('pkgconfig')
pkg.generate(
  name: 'atomic_queue',
  description: 'C++14 multiple-producer-multiple-consumer lock-free queues based on circular buffers',
  libraries: [threads_dep]
)
