# Changelog

All notable changes to this project are documented in this file.

## [1.9.4] - 2026-09-16

### Fixed
- Compilation errors with `-std=c++20` and `-std=c++23`.
- Meson build with Apple clang, which has no libatomic (#108).
- `AtomicQueueB2` rollback helper C++20 constructor error (#110).

### Changed
- Makefile CI now compiles with C++14, C++20 and C++23 standards.
- Makefile CI retired ubuntu-22.04 runners.

## [1.9.3] - 2026-09-15

### Fixed
- Cache line size on Apple Silicon (#106).
- `cpu_base_frequency` detection on AMD and ARM.
- Benchmark chart tooltips broken by a Highcharts update.
- `AtomicQueueB2` constructor is now strongly exception-safe (cleans up on
  allocation/construction failure).

### Added
- Capacity-limit assertions.
- `[[nodiscard]]` on `try_push` and `try_pop`.
- Better `is_always_lock_free` assertion under C++17.
- `constructor_strong_exception_safety` unit tests (including allocation-size checks).
- Cross-architecture CI under QEMU: ppc64, s390x, riscv, loongarch (#107).
- Minimal 4-CPU benchmark to Makefile, CMake, and Meson CI; `std::mutex`
  baseline in the minimal benchmark.
- Continuous Integrations status table and thread-cancellation notes in README.

### Changed
- Propagate `-latomic` dependency into Meson `pkg.generate`.
- Tighter GCC / GCC-ARM codegen flags; retired `-Wno-unused-variable` and
  `-Wpedantic` workarounds.
- Parse `/proc/cpuinfo` in a single pass.
- Refreshed Ryzen 5950X and 5825U benchmark results.

### Dependencies
- Bump `actions/checkout` from 6 to 7 (#105).

[1.9.4]: https://github.com/max0x7ba/atomic_queue/compare/v1.9.3...v1.9.4
[1.9.3]: https://github.com/max0x7ba/atomic_queue/compare/v1.9.2...v1.9.3
