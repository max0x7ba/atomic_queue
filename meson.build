# Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

# (rm -rf build; meson setup build; time ninja -C build -v)
#
# export CXX=g++; rm -rf /tmp/build; cd ~/src/atomic_queue && meson setup /tmp/build -Dwerror=true -Dwarning_level=2 -Dbenchmarks=true && meson compile -vC /tmp/build && meson test -vC /tmp/build --print-errorlogs

project(
  'atomic_queue', 'cpp',
  version : '1.9.2',
  license : 'MIT',
  default_options : ['cpp_std=c++14', 'buildtype=release', 'b_ndebug=if-release']
)

cxx = meson.get_compiler('cpp')

atomic_queue_deps = [dependency('threads')]

# clang++ does not always inline atomic operations and may emit calls to libatomic, so link with it explicitly.
if cxx.get_id() == 'clang'
  atomic_queue_deps += cxx.find_library('atomic')
endif

atomic_queue_dep = declare_dependency(include_directories : ['include'], dependencies : atomic_queue_deps)
meson.override_dependency('atomic_queue', atomic_queue_dep)

if get_option('tests')
  unit_test_framework_dep = dependency('boost', modules : ['unit_test_framework'])

  tests_exe = executable(
    'tests',
    'src/tests.cc',
    dependencies : [atomic_queue_dep, unit_test_framework_dep],
  )
  test('tests', tests_exe, args : ['--log_level=unit_scope', '--report_level=short'], is_parallel : false)

  example_exe = executable(
    'example',
    'src/example.cc',
    dependencies : [atomic_queue_dep],
  )
  test('example', example_exe, is_parallel : false)
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
    dependencies : [atomic_queue_dep, dl_dep, xenium_dep, boost_dep, tbb_dep, moodycamel_dep],
    override_options : ['cpp_std=c++17'] # Benchmarks require C++17 or higher
  )
  benchmark('benchmarks', benchmarks_exe, timeout: 3600)
endif

if get_option('benchmarks_min')
  dl_dep = cxx.find_library('dl', required : false)
  boost_dep = dependency('boost')
  benchmarks_min_exe = executable(
    'benchmarks_min',
    ['src/benchmarks_min.cc', 'src/cpu_base_frequency.cc', 'src/huge_pages.cc'],
    dependencies : [atomic_queue_dep, dl_dep]
  )
  benchmark('benchmarks_min', benchmarks_min_exe, timeout: 300)
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
  url: 'https://github.com/max0x7ba/atomic_queue/',
  version: meson.project_version(),
  libraries: [atomic_queue_deps],
)
