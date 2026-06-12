# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

# (rm -rf build; meson setup build; time ninja -C build -v)
#
# cd ~/src/atomic_queue && rm -rf build && meson setup build -Dwerror=true -Dwarning_level=2 -Dbenchmarks=true && meson compile -C build && meson test -C build --print-errorlogs

project(
  'atomic_queue', 'cpp',
  version : '1.9.1',
  license : 'MIT License',
  default_options : ['cpp_std=c++14', 'buildtype=release', 'b_ndebug=if-release']
)

cxx = meson.get_compiler('cpp')

threads_dep = dependency('threads')

atomic_queue_dep = declare_dependency(include_directories : ['include'], dependencies : threads_dep)
meson.override_dependency('atomic_queue', atomic_queue_dep)

if get_option('tests')
  wno_unused_variable = cxx.get_supported_arguments('-Wno-unused-variable')

  unit_test_framework_dep = dependency('boost', modules : ['unit_test_framework'])

  tests_exe = executable(
    'tests',
    'src/tests.cc',
    cpp_args : wno_unused_variable,
    dependencies : [atomic_queue_dep, unit_test_framework_dep],
  )
  test('tests', tests_exe)

  example_exe = executable(
    'example',
    'src/example.cc',
    cpp_args : wno_unused_variable,
    dependencies : [atomic_queue_dep],
  )
  test('example', example_exe)
endif

if get_option('benchmarks')
  # libdl is folded into libc on modern glibc (>= 2.34), so it may not exist as a separate library even though dlsym() links fine.
  dl_dep = cxx.find_library('dl', required : false)
  xenium_dep = declare_dependency(include_directories : '../xenium')
  boost_dep = dependency('boost')
  # Prefer the packaged TBB dependency (pkg-config/CMake config) and fall back to a plain library lookup if it is not discoverable.
  tbb_dep = dependency('tbb', required : false)
  if not tbb_dep.found()
    tbb_dep = cxx.find_library('tbb')
  endif
  moodycamel_dep = declare_dependency(include_directories : '../')

  benchmarks_exe = executable(
    'benchmarks',
    ['src/benchmarks.cc', 'src/cpu_base_frequency.cc', 'src/huge_pages.cc'],
    include_directories : ['src'],
    dependencies : [atomic_queue_dep, dl_dep, xenium_dep, boost_dep, tbb_dep, moodycamel_dep],
    override_options : ['cpp_std=c++17'] # Benchmarks require C++17 or higher
  )
  benchmark('benchmarks', benchmarks_exe)
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
  description: 'C++14 concurrent lock-free low-latency queue',
  version: meson.project_version(),
  libraries: [threads_dep]
)
