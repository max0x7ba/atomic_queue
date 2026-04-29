[![C++14](https://img.shields.io/badge/dialect-C%2B%2B14-blue)](https://en.cppreference.com/w/cpp/14)
[![MIT license](https://img.shields.io/github/license/max0x7ba/atomic_queue)](https://github.com/max0x7ba/atomic_queue/blob/master/LICENSE)
[![Latest release](https://img.shields.io/github/v/tag/max0x7ba/atomic_queue?label=latest%20release)](https://github.com/max0x7ba/atomic_queue/releases/tag/v1.9.0)
[![Conan Center](https://img.shields.io/conan/v/atomic_queue)](https://conan.io/center/recipes/atomic_queue)
[![Vcpkg Version](https://img.shields.io/vcpkg/v/atomic-queue)](https://vcpkg.io/en/package/atomic-queue)
<br>
[![Makefile Continuous Integrations](https://github.com/max0x7ba/atomic_queue/actions/workflows/ci.yml/badge.svg)](https://github.com/max0x7ba/atomic_queue/actions/workflows/ci.yml)
[![CMake Continuous Integrations](https://github.com/max0x7ba/atomic_queue/actions/workflows/cmake-gcc-clang.yml/badge.svg)](https://github.com/max0x7ba/atomic_queue/actions/workflows/cmake-gcc-clang.yml)
[![Meson Continuous Integrations](https://github.com/max0x7ba/atomic_queue/actions/workflows/ci-meson.yml/badge.svg)](https://github.com/max0x7ba/atomic_queue/actions/workflows/ci-meson.yml)
<br>
![platform Linux x86_64](https://img.shields.io/badge/platform-Linux%20x86_64--bit-gold)
![platform Linux ARM](https://img.shields.io/badge/platform-Linux%20ARM-gold)
![platform Linux RISC-V](https://img.shields.io/badge/platform-Linux%20RISC--V-gold)
![platform Linux PowerPC](https://img.shields.io/badge/platform-Linux%20PowerPC-gold)
![platform Linux IBM System/390](https://img.shields.io/badge/platform-Linux%20IBM%20System/390-gold)
![platform Linux LoongArch](https://img.shields.io/badge/platform-Linux%20LoongArch-gold)
![platform Windows x86_64](https://img.shields.io/badge/platform-Windows%20x86_64--bit-gold)

# atomic_queue
C++14 multiple-producer-multiple-consumer *lock-free* queues based on circular buffers and [`std::atomic`][3].

Designed with a goal to minimize the latency between one thread pushing an element into a queue and another thread popping it from the queue.

It has been developed, tested and benchmarked on Linux. Yet, any C++14 platform implementing `std::atomic` is expected to compile the unit-tests and run them without failures just as well.

Continuous integrations running the unit-tests on GitHub are set up for x86_64 and arm64 platforms, Ubuntu-22.04, Ubuntu-24.04 and Windows. Pull requests to extend the [continuous integrations][18] to run the unit-tests on other architectures/platforms are most welcome.

## Design Principles
When minimizing latency a good design is not when there is nothing left to add, but rather when there is nothing left to remove, as these queues exemplify.

Minimizing latency naturally maximizes throughput. Low latency reciprocal is high throughput, in ideal mathematical and practical engineering sense. Low latency is incompatible with any delays and/or batching, which destroy original (hardware) global time order of events pushed into one queue by different threads. Maximizing throughput, on the other hand, can be done at expense of latency by delaying and batching multiple updates.

The main design principle these queues follow is _minimalism_, which results in such design choices as:

* Bare minimum of atomic instructions. Inlinable by default push and pop functions can hardly be any cheaper in terms of CPU instruction number / L1i cache pressure.
* Explicit contention/false-sharing avoidance for queue data members and its elements.
* Linear fixed size ring-buffer array. No heap memory allocations after a queue object has constructed. It doesn't get any more CPU L1d or TLB cache friendly than that.
* Value semantics. Meaning that the queues make a copy/move upon `push`/`pop` and keep no references/pointers to its function arguments after returning, and that no reference/pointer to elements in the queue ring-buffer can be obtained. Simplest to use, hard to misuse, best machine code due to no pointer aliasing possible.

The impact of each of these small design choices on their own is barely measurable, but their total impact is much greater than a simple sum of the constituents' impacts, aka super-scalar compounding or synergy. The synergy emerging from combining multiple of these small design choices together is what allows CPUs to perform at their peak capacities least impeded.

These design choices are also limitations:

* The maximum queue size must be set at compile time or construction time. The circular buffer side-steps the memory reclamation problem inherent in linked-list based queues for the price of fixed buffer size. See [Effective memory reclamation for lock-free data structures in C++][4] for more details. Fixed buffer size may not be that much of a limitation, since once the queue gets larger than the maximum expected size that indicates a problem that elements aren't consumed fast enough, and if the queue keeps growing it may eventually consume all available memory which may affect the entire system, rather than the problematic process only. The only apparent inconvenience is that one has to do an upfront calculation on what would be the largest expected/acceptable number of unconsumed elements in the queue.
* There are no OS-blocking push/pop functions. This queue is designed for ultra-low-latency scenarios and using an OS blocking primitive would be sacrificing push-to-pop latency. For lowest possible latency one cannot afford calling the OS kernel or blocking in the OS kernel because the wake-up latency of a blocked thread is about 1-3 microseconds, whereas this queue's round-trip time (push a message to another thread and pop its reply) can be below 100 nanoseconds. CPU vulnerability mitigations made system calls dramatically more expensive, crippling performance even worse. In general, handing off spin-waiting to an OS blocking primitive is a problem with no satisfactory low-latency solutions.

Ultra-low-latency applications need just that and nothing more. The minimalism pays off, see the [throughput and latency benchmarks][1].

## Role Models
Several other well established and popular thread-safe containers are used for reference in the [benchmarks][1]:

| Queue | Type | Description |
|-------|------|-------------|
| `std::mutex` | MPMC | A fixed size ring-buffer with `std::mutex`. |
| `pthread_spinlock` | MPMC | A fixed size ring-buffer with `pthread_spinlock_t`. |
| `boost::lockfree::spsc_queue` | SPSC | A wait-free queue from Boost library. |
| `boost::lockfree::queue` | MPMC | A lock-free queue from Boost library. |
| `moodycamel::ConcurrentQueue` | MPMC | A lock-free queue used in non-blocking mode. Designed to maximize throughput at the expense of latency, eschewing global time order. Not equivalent to other queues benchmarked here in this respect. |
| `moodycamel::ReaderWriterQueue` | SPSC | A lock-free queue used in non-blocking mode. |
| `xenium::michael_scott_queue` | MPMC | A lock-free queue proposed by [Michael and Scott](http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf) (similar to `boost::lockfree::queue` which is also based on the same proposal). |
| `xenium::ramalhete_queue` | MPMC | A lock-free queue proposed by [Ramalhete and Correia](http://concurrencyfreaks.blogspot.com/2016/11/faaarrayqueue-mpmc-lock-free-queue-part.html). |
| `xenium::vyukov_bounded_queue` | MPMC | A bounded queue based on the version proposed by [Vyukov](https://groups.google.com/forum/#!topic/lock-free/-bqYlfbQmH0). |
| `tbb::spin_mutex` | MPMC | A locked fixed size ring-buffer with `tbb::spin_mutex` from Intel Threading Building Blocks. |
| `tbb::concurrent_bounded_queue` | MPMC | Eponymous queue used in non-blocking mode from Intel Threading Building Blocks. |

## Using the library
The containers provided are header-only class templates, no building/installing is necessary.

### Installing
#### From GitHub
1. Clone the project:
   ```bash
   git clone https://github.com/max0x7ba/atomic_queue.git
   ```
2. Add `atomic_queue/include` directory (use full path) to the include paths of your build system.
3. `#include <atomic_queue/atomic_queue.h>` in your C++ source.

If you use CMake, these can be simplified as follows:
```cmake
add_subdirectory(atomic_queue)
target_link_libraries(main PRIVATE atomic_queue::atomic_queue)
```

#### From GitHub using cmake FetchContent
You can also use CMake's FetchContent.
```cmake
include(FetchContent)
FetchContent_Declare(
        atomic_queue
        GIT_REPOSITORY https://github.com/max0x7ba/atomic_queue.git
        GIT_TAG master
)
FetchContent_MakeAvailable(atomic_queue)
target_link_libraries(main PRIVATE atomic_queue::atomic_queue)
```

#### From vcpkg
```
vcpkg install atomic-queue
```
It provides CMake targets:
```cmake
find_package(atomic_queue CONFIG REQUIRED)
target_link_libraries(main PRIVATE atomic_queue::atomic_queue)
```

#### Install using conan
Follow the official tutorial on [how to consume conan packages](https://docs.conan.io/2/tutorial/consuming_packages.html).
Details specific to this library are available in [ConanCenter](https://conan.io/center/recipes/atomic_queue).

### Build and run
Building is necessary to run the unit-tests and benchmarks.

GNU Make `Makefile` is the original/reference build system this project has been developed, unit-tested and benchmarked with. It is feature-complete and most efficient, but the least portable to anything else than Linux. Linux tools, however, are the ultimate best for development, benchmarking, deep performance analyses and profiling at CPU instruction level.

`CMake` and `Meson` build systems provide the greatest portability and easy usage/consumption of the library, build and run the unit-tests on their supported platforms. They provide the best library user experience, as opposed to best library developer experience.

Some most desirable developer features in GNU Make not found elsewhere:

* Ultimate totalitarian control of compilers, tools and their options with no friction or indirection. Having to invoke CMake or Meson is undesirable extra friction.
* Unconditionally robust builds, which never require invoking `make clean`. That's possible only when tracking complete actual compiler and linker options, which this `Makefile` does at 0-cost.
* Fast parallel builds and unit-test runs, unmatched by anything else, for rapid R&D cycles.
* Fast _**multi-toolset parallel builds**_ and unit-test runs, with no idling CPUs at all times, for fastest possible exhaustive pre-deployment tests. That's impossible for/with build file generators, for `ninja` in principle, and/or anything else.

GNU Make command line options with the greatest impact on built time:

* `-j` sets the number of worker processes to build with in parallel. `-j$(($(nproc)/2))` sets it to half the number of available CPUs.
* `-R` disables GNU Make legacy built-in variables and rules for at least +25% faster dependency checking. `make -R` is the right default invocation for GNU Make. Not specifying `-R` command line option for GNU Make is wasting time and energy for no good reason. (`ninja` docs never mention `make -R` because `ninja` is unable to build as fast as `make -R` does.)

#### Build and run unit-tests
Building and running the unit-tests require Boost.Test library (e.g. `libboost-test-dev` on Debian/Ubuntu). Installing the complete set of Boost development libraries is the easiest (e.g. `libboost-all-dev` on Debian/Ubuntu).

```bash
git clone https://github.com/max0x7ba/atomic_queue.git
cd atomic_queue
```
After succeeding the above commands,
```
# Build and run the unit-tests with gcc (default).
make -R -j$(($(nproc)/2)) BUILD=debug run_tests

# Build and run the unit-tests with gcc,gcc-14,clang,clang-20 in parallel.
make -R -j$(($(nproc)/2)) BUILD=debug TOOLSET=gcc,gcc-14,clang,clang-20 run_tests
```

#### Build and run benchmarks
Building and running the benchmarks require additional third-party libraries:
* Boost.Lockfree library. Installing the complete set of Boost development libraries is the easiest (e.g. `libboost-all-dev` on Debian/Ubuntu).
* Intel TBB library (e.g. `libtbb-dev` on Debian/Ubuntu). When Intel TBB library is installed elsewhere, you may like to specify that location in `cppflags.tbb` and `ldlibs.tbb` in `Makefile`.
* Several other third-party libraries are expected to be cloned into sibling directories:

```bash
git clone https://github.com/cameron314/concurrentqueue.git
git clone https://github.com/cameron314/readerwriterqueue.git
git clone https://github.com/mpoeter/xenium.git

git clone https://github.com/max0x7ba/atomic_queue.git
cd atomic_queue
```

After succeeding the above commands,

```
make -R -j$(($(nproc)/2)) run_benchmarks_n       # Build and run the benchmarks once.
make -R -j$(($(nproc)/2)) run_benchmarks_n N=3   # Build and run the benchmarks 3 times.

make -R -j$(($(nproc)/2)) TOOLSET=gcc-14 run_benchmarks_n    # Build with gcc-14 and run the benchmarks.
make -R -j$(($(nproc)/2)) TOOLSET=clang-20 run_benchmarks_n  # Build with clang-20 and run the benchmarks.

taskset --cpu-list 0,1,14,15 make -R -j$(($(nproc)/2)) TOOLSET=gcc-14 run_benchmarks_n  # Use only cpus [0,1,14,15] to build with gcc-14 and run the benchmarks 3 times.
```

## Library contents
### Available queues
* `AtomicQueue` - a fixed size ring-buffer for atomic elements.
* `OptimistAtomicQueue` - a faster fixed size ring-buffer for atomic elements which busy-waits when empty or full. It is `AtomicQueue` used with `push`/`pop` instead of `try_push`/`try_pop`.
* `AtomicQueue2` - a fixed size ring-buffer for non-atomic elements.
* `OptimistAtomicQueue2` - a faster fixed size ring-buffer for non-atomic elements which busy-waits when empty or full. It is `AtomicQueue2` used with `push`/`pop` instead of `try_push`/`try_pop`.

These containers maintain their ring-buffers as array data members with size specified at compile-time and have no pointer data members. That makes them position-independent, allows allocating them into process-shared memory with a plain C++ placement new statement, and mapping at arbitrary addresses in different processes using the same queue objects in shared memory. The queue elements must be position-independent too to support this particular use-case (unlike classes with process-position-dependent pointers such as `std::unique_ptr`, `std::string` and all the C++ standard containers with default allocators).

There are corresponding `B` variants (`AtomicQueueB`, `OptimistAtomicQueueB`, `AtomicQueueB2`, `OptimistAtomicQueueB2`) that use `std::allocator` or user-specified (stateful) allocator for allocating the ring-buffers, where the buffer size is specified as an argument to the constructor at run-time.

Totally ordered mode is supported. In this mode consumers receive messages in the same FIFO order the messages were posted. This mode is supported for `push` and `pop` functions, but not for the `try_` versions. On Intel x86 the totally ordered mode has 0 cost, as of 2019.

Single-producer-single-consumer mode is supported. In this mode, no expensive atomic read-modify-write CPU instructions are necessary, only the cheapest atomic loads and stores. That improves queue throughput significantly.

Move-only queue element types are fully supported. For example, a queue of `std::unique_ptr<T>` elements would be `AtomicQueueB2<std::unique_ptr<T>>` or `AtomicQueue2<std::unique_ptr<T>, CAPACITY>`.

### Queue schematics

```
queue-end                 queue-front
[newest-element, ..., oldest-element]
push()                          pop()
```

### Queue API
The queue class templates provide the following member functions:
* `try_push` - Appends an element to the end of the queue. Returns `false` when the queue is full.
* `try_pop` - Removes an element from the front of the queue. Returns `false` when the queue is empty.
* `push` (optimist) - Appends an element to the end of the queue. Busy waits when the queue is full. Faster than `try_push` when the queue is not full. Optional FIFO producer queuing and total order.
* `pop` (optimist) - Removes an element from the front of the queue. Busy waits when the queue is empty. Faster than `try_pop` when the queue is not empty. Optional FIFO consumer queuing and total order.
* `was_size` - Returns the number of unconsumed elements during the call. The state may have changed by the time the return value is examined.
* `was_empty` - Returns `true` if the container was empty during the call. The state may have changed by the time the return value is examined.
* `was_full` - Returns `true` if the container was full during the call. The state may have changed by the time the return value is examined.
* `capacity` - Returns the maximum number of elements the queue can possibly hold.

_Atomic elements_ are those, for which [`std::atomic<T>{T{}}.is_lock_free()`][10] returns `true`, and, when C++17 features are available, [`std::atomic<T>::is_always_lock_free`][16] evaluates to `true` at compile time. In other words, the CPU can load, store and compare-and-exchange such elements atomically natively. On x86-64 such elements are all the [C++ standard arithmetic and pointer types][11].

The queues for atomic elements reserve one value to serve as an empty element marker `NIL`, its default value is `0`. `NIL` value must not be pushed into a queue and there is an [`assert`][13] statement in `push` functions to guard against that in debug mode builds. Pushing `NIL` element into a queue in release mode builds results in undefined behaviour, such as deadlocks and/or lost queue elements.

Note that _optimism_ is a choice of a queue modification operation control flow, rather than a queue type. An _optimist_ `push` is fastest when the queue is not full most of the time, an optimistic `pop` - when the queue is not empty most of the time. Optimistic and not so operations can be mixed with no restrictions. The `OptimistAtomicQueue`s in [the benchmarks][1] use only _optimist_ `push` and `pop`.

See [example.cc](src/example.cc) for a usage example.

## Implementation Notes
### Memory order of non-atomic loads and stores
`push` and `try_push` operations _synchronize-with_ (as defined in [`std::memory_order`][17]) with any subsequent `pop` or `try_pop` operation of the same queue object. Meaning that:
* No non-atomic load/store gets reordered past `push`/`try_push`, which is a `memory_order::release` operation. Same memory order as that of `std::mutex::unlock`.
* No non-atomic load/store gets reordered prior to `pop`/`try_pop`, which is a `memory_order::acquire` operation. Same memory order as that of `std::mutex::lock`.
* The effects of a producer thread's non-atomic stores followed by `push`/`try_push` of an element into a queue become visible in the consumer's thread which `pop`/`try_pop` that particular element.

### Ring-buffer capacity
The available queues here use a ring-buffer array for storing elements. The capacity of the queue is fixed at compile time or construction time.

In a production multiple-producer-multiple-consumer scenario the ring-buffer capacity should be set to the maximum expected queue size. When the ring-buffer gets full it means that the consumers cannot consume the elements fast enough. A fix for that is any of:

* Increase the queue capacity in order to handle temporary spikes of pending elements in the queue. This normally requires restarting the application after re-configuration/re-compilation has been done.
* Increase the number of consumers to drain the queue faster. The number of consumers can be managed dynamically, e.g.: when a consumer observes that the number of elements pending in the queue keeps growing, that calls for deploying more consumer threads to drain the queue at a faster rate; mostly empty queue calls for suspending/terminating excess consumer threads.
* Decrease the rate of pushing elements into the queue. `push` and `pop` calls always incur some expensive CPU cycles to maintain the integrity of queue state in atomic/consistent/isolated fashion with respect to other threads and these costs increase super-linearly as queue contention grows. Producer batching of multiple small elements or elements resulting from one event into one queue message is often a reasonable solution.

Using a power-of-2 ring-buffer array size allows a couple of important optimizations:

* The writer and reader indexes get mapped into the ring-buffer array index using remainder binary operator `% SIZE`. Remainder binary operator `%` normally generates a division CPU instruction which isn't cheap, but using a power-of-2 size turns that remainder operator into one cheap binary `and` CPU instruction and that is as fast as it gets.
* The *element index within the cache line* gets swapped with the *cache line index*, so that consecutive queue elements get mapped into consecutive/distinct cache lines. This massively reduces cache line contention between multiple producers and multiple consumers. Instead of `N` producers together with `M` consumers competing on subsequent elements in the same ring-buffer cache line in the worst case, it is only one producer competing with one consumer (pedantically, when the number of CPUs is not greater than the number of elements that can fit in one cache line). This optimisation scales better with the number of producers and consumers, and element size. With low number of producers and consumers (up to about 2 of each in these benchmarks) disabling this optimisation may yield better throughput (but higher variance across runs).

The containers use `unsigned` type for size and internal indexes. On x86-64 platform `unsigned` is 32-bit wide, whereas `size_t` is 64-bit wide. 64-bit instructions utilise an extra byte instruction prefix resulting in slightly more pressure on the CPU instruction cache and the front-end. Hence, 32-bit `unsigned` indexes are used to maximise performance. That limits the queue size to 4,294,967,295 elements, which seems to be a reasonable hard limit for many applications.

While the atomic queues can be used with any moveable element types (including `std::unique_ptr`), for best throughput and latency the queue elements should be cheap to copy and lock-free (e.g. `unsigned` or `T*`), so that `push` and `pop` operations complete fastest.

### Lock-free guarantees
*Conceptually*, a `push` or `pop` operation does two atomic steps:

1. Atomically and exclusively claims the queue slot index to store/load an element to/from. That's producers incrementing `head` index, consumers incrementing `tail` index. Each slot is accessed by one producer and one consumer threads only.
2. Atomically store/load the element into/from the slot. Producer storing into a slot changes its state to be non-`NIL`, consumer loading from a slot changes its state to be `NIL`. The slot is a spinlock for its one producer and one consumer threads.

These queues anticipate that a thread doing `push` or `pop` may complete step 1 and then be preempted before completing step 2.

When a thread completes step 1 and terminates (for any reason) without completing step 2, the queue slot remains locked and deadlocks the next thread attempting to `try_pop`/`try_push`/`pop`/`push` from/into that slot. A thread can be terminated by the OS (e.g., `oomkiller`), or throw/crash in the user-defined copy/move constructor/assignment of queue element (if any). Should that happen, the game is over, and the best course of action is to terminate the process as soon as possible and address the root cause of one's threads crashing.

Once constructed/allocated, queue objects maintain their invariants and never throw exceptions, provided that:
* Queue elements copy/move constructor/assignment never throw.
* Threads don't terminate half-way through `push`/`pop`.
* Thread preemption half-way through `push`/`pop` is not great, not terrible. Real-time FIFO threads with real-time thread throttling disabled are required for best results.

An algorithm is *lock-free* if there is guaranteed system-wide progress. These queues guarantee system-wide progress by the following properties:

* Each `push` is independent of any preceding `push`. An incomplete (preempted) `push` by one producer thread doesn't affect `push` of any other thread.
* Each `pop` is independent of any preceding `pop`. An incomplete (preempted) `pop` by one consumer thread doesn't affect `pop` of any other thread.
* An incomplete (preempted) `push` from one producer thread affects only one consumer thread `pop`ing an element from this particular queue slot. All other threads `pop`s are unaffected.
* An incomplete (preempted) `pop` from one consumer thread affects only one producer thread `push`ing an element into this particular queue slot while expecting it to have been consumed long time ago, in the rather unlikely scenario that producers have wrapped around the entire ring-buffer while this consumer hasn't completed its `pop`. All other threads `push`s and `pop`s are unaffected.

### Preemption
Linux task scheduler thread preemption is something no user-space process should be able to affect or escape, otherwise any/every malicious application would exploit that.

Still, there are a few things one can do to minimize preemption of one's mission critical application threads:

* Use real-time `SCHED_FIFO` scheduling class for your threads, e.g. `chrt --fifo 50 <app>`. A higher priority `SCHED_FIFO` thread or kernel interrupt handler can still preempt your `SCHED_FIFO` threads.
* Use one same fixed real-time scheduling priority for all threads accessing same queue objects. Real-time threads with different scheduling priorities modifying one queue object may cause priority inversion and deadlocks. Using the default scheduling class `SCHED_OTHER` with its dynamically adjusted priorities defeats the purpose of using these queues.
* Disable [real-time thread throttling](#real-time-thread-throttling) to prevent `SCHED_FIFO` real-time threads from being throttled.
* Isolate CPU cores, so that no interrupt handlers or applications ever run on it. Mission critical applications should be explicitly placed on these isolated cores with `taskset`.
* Pin threads to specific cores, otherwise the task scheduler keeps moving threads to other idle CPU cores to level voltage/heat-induced wear-and-tear across CPU cores. Keeping a thread running on one same CPU core maximizes CPU cache hit rate. Moving a thread to another CPU core incurs otherwise unnecessary CPU cache thrashing.

People often propose limiting busy-waiting with a subsequent call to `std::this_thread::yield()`/`sched_yield`/`pthread_yield`. However, `sched_yield` is a wrong tool for locking because it doesn't communicate to the OS kernel what the thread is waiting for, so that the OS thread scheduler can never schedule the calling thread to resume at the right time when the shared state has changed (unless there are no other threads that can run on this CPU core, so that the caller resumes immediately). See notes section in [`man sched_yield`][19] and [a Linux kernel thread about `sched_yield` and spinlocks][5] for more details.

[In Linux, there is mutex type `PTHREAD_MUTEX_ADAPTIVE_NP`][9] which busy-waits a locked mutex for a number of iterations and then makes a blocking syscall into the kernel to deschedule the waiting thread. In the benchmarks it was the worst performer and I couldn't find a way to make it perform better, and that's the reason it is not included in the benchmarks.

C++20 introduced blocking `std::atomic::wait` which uses Linux futex for atomic compare-and-block operation, similar to `PTHREAD_MUTEX_ADAPTIVE_NP`, with hard-coded spin-count limits. And `thread_yield` calls, which Linus Torvalds above explains is only applicable for real-time threads with a single run-queue for them. A queue implementation with `std::atomic::wait` is due to be benchmarked, with its performance expected to be similar to that of `PTHREAD_MUTEX_ADAPTIVE_NP`, but I'd love to be pleasantly surprised.

On Intel CPUs one could use [the 4 debug control registers][6] to monitor the spinlock memory region for write access and wait on it using `select` (and its friends) or `sigwait` (see [`perf_event_open`][7] and [`uapi/linux/hw_breakpoint.h`][8] for more details). A spinlock waiter could suspend itself with `select` or `sigwait` until the spinlock state has been updated. But there are only 4 of these registers, so that such a solution wouldn't scale.

### Huge pages
Using huge pages improves performance of memory intensive applications dramatically. The benchmark tries allocating 1GB or 2MB huge pages first, and falls back to allocating the default tiny pages (4kB on x86_64). The benchmark results are reproducible only when it succeeds allocating one 1GB huge page.

Using smaller pages cripple CPU performance with TLB cache misses.

## Benchmarks
[View throughput and latency benchmarks charts][1].

### Methodology
There are a few OS behaviours that complicate benchmarking, make benchmark runs irreproducible, and introduce otherwise unnecessary timing noise into benchmark timings:

* CPU scheduler can place threads on different CPU cores each run, often poorly. The benchmark pins threads to specific CPU cores to test throughput/latency of communication between in particular scenarios, such as SMT, cross-core with no SMT, cross-CCX/socket. That obviates the CPU scheduler from having to decide which CPU to run a thread on completely, along with any of its mis-scheduling risks[^3].
* CPU scheduler preempts threads. Real-time `SCHED_FIFO` priority 50 is used to make benchmark threads non-preemptable by lower priority processes/threads.
* Real-time thread throttling, enabled by default, makes the kernel preempt real-time `SCHED_FIFO` threads every second to protect against the risk of real-time threads hogging all CPUs and making the system unavailable for anything else. Disabled during benchmarks runs[^1].
* Address space randomisation makes CPU branch prediction the least efficient. In addition to making all CPU caches only "somewhat less" but not the least efficient. Disabled during benchmarks runs[^1].
* CPU vulnerability mitigations make system calls dramatically more expensive. Disabled during benchmarks runs[^1].
* Transparent huge pages are "always" enabled during benchmarks runs[^2]. That enables only much less than 50% of transparent huge page functionality and its benefits, with (indefinitely) delayed effect.
* Synchronous compaction is "always" enabled during benchmarks runs[^2]. Enables the rest of transparent huge page functionality and its benefits, with immediate positive effect on memory allocations in every process. [thp-usage][14] provides more information.

To further minimise the noise in timings:
* `benchmarks` executable runs the same workload 10 times with each queue and reports the shortest (best) time measured. Everyone needs a break every little while and `benchmarks` executable gives each queue 10 breaks every few fractions of a second.
* The benchmark charts report `{mean, stdev, min, max}` descriptive statistics of the best throughput `msg/sec` and latency `sec/round-trip` measurements obtained from at least 33 runs of `benchmarks` executable. (33 runs of `benchmarks` take ~1h30s on Ryzen 5950X).

The goal of the benchmarks is to time things as accurately as possible. For the purpose of viewing the reality clearly without any distortions of perception, prejudice or judgement.

#### Huge pages
When huge pages are available the benchmarks use 1x1GB or 16x2MB huge pages for the queues to minimise TLB misses. To enable huge pages do one of:
```bash
sudo hugeadm --pool-pages-min 1GB:1
sudo hugeadm --pool-pages-min 2MB:32
```
Alternatively, you may like to enable [transparent hugepages][15] in your system and/or use a hugepage-aware allocator. [thp-usage][14] provides more information.

#### Real-time thread throttling
By default, Linux scheduler throttles real-time threads from consuming 100% of CPU and that is detrimental to benchmarking. Full details can be found in [Real-Time group scheduling][2]. To disable real-time thread throttling do:
```bash
echo -1 | sudo tee /proc/sys/kernel/sched_rt_runtime_us >/dev/null
```

### Throughput and scalability benchmark
N producer threads push a 4-byte integer into one same queue, N consumer threads pop the integers from the queue. Each producer posts 1,000,000 messages. Total time taken to send and receive all these messages is measured.

With SMT threads, the benchmark is run for from 1 producer and 1 consumer up to `(total-number-of-cpus / 2)` producers/consumers to measure the scalability of different queues. Without using SMT threads (cross-core communication only) -- up to `(total-number-of-cpus / 4)` producers/consumers.

A benchmark run reports the best msg/sec throughput out of 10 tries for each queue.

The charts report mean, stdev, min and max of msg/sec throughput across 33 benchmark runs.

### Ping-pong benchmark
One thread posts an integer to another thread through one queue and waits for a reply from another queue, 2 queues and 2 threads, in total. A _ping_ is an incoming message, a _pong_ is a reply to that. Only one `ping` or `pong` message exists or is being in-flight, at all times. In total, each thread `push`es 500,000 _pings_ into its ingress queue, and `pop`s 500,000 pongs from its ingress queue.

Contention is minimal here (1-producer-1-consumer, 1 element in the queue) in order to be able to achieve the lowest possible latency a queue could possibly deliver.

This benchmark measures the total time taken to post 500,000 messages and receive 500,000 replies.

A benchmark run reports the best sec/round-trip latency (time taken to `push` a message and `pop` its reply) out of 10 tries for each queue.

The charts report mean, stdev, min and max of sec/round-trip latency across 33 benchmark runs.

## Benchmarks Notes
- The lowest latency for cross-thread communication is achieved when both threads run on the **same CPU core** (2 SMT threads). Moving communication to a different core adds noticeable latency, and crossing CCX boundaries or CPU sockets increases it further.
- In the round-trip latency benchmark, **every tested queue** achieves its best latency only when the producer and consumer threads share the same CPU core.
When the threads run on **different cores**, latency increases by **3× or more**.
- When producer and consumer threads run on different cores, **all queues** show **at least 1.5× lower** throughput.
- The numbers shown for each queue reflect the **best-case** result across within-core, cross-core, and cross-CCX/socket scenarios.
- **Important**: These scenarios behave very differently in practice. Excellent performance with two SMT threads on a single core is often irrelevant for real-world use cases where threads must run on different cores.
- **Recommendation**: Always benchmark your specific thread placement (same core vs. different cores vs. different CCX) for latency-critical or throughput-critical applications.

### Notable exceptions
`moodycamel::ConcurrentQueue` is the notable exception here, with its 1-producer-1-consumer throughput being the worst, yet scaling up in almost perfect linear fashion when the benchmark adds another pair of producer-consumer threads.

That is quite unlike anything and makes one think of possible root causes for its extreme top throughput performance diverging from anything else.

Thinking more about it, reveals `moodycamel::ConcurrentQueue` architecture at a glance, without having to examine its source code -- it is a bunch of single-producer-single-consumer queues trying to emulate a multiple-producer-multiple-consumer queue, where different threads `push` into different internal SPSC queues, different threads `pop` from different internal SPSC queues. It is unable to `pop` elements in the same _global time order_ matching that of _global time order_ of `push` calls, unlike true MPMC queues. Neither it is able to match the _low latencies_ of true SPSC queues.

`moodycamel::ConcurrentQueue` probably aspired to combine the _global time order_ feature of MPMC queues with the _low-latency_ feature of SPSC queues. But ended up delivering neither the _global time order_ nor the _low-latency_.

`moodycamel::ConcurrentQueue` could still be, technically, called an MPMC queue. While, in practice, not being a feasible/fungible/compatible drop-in replacement for all other (true) MPMC queue benchmarked here. Whereas all the other MPMC queues are perfectly fungible compatible drop-in replacements for any other MPMC queue, including pseudo-MPMC `moodycamel::ConcurrentQueue` too.

`moodycamel::ConcurrentQueue` is optimized for throughput at the expense of anything else, it appears, thusly epitomizing ["_When a measure becomes a target, it ceases to be a good measure_"][20] better than anything else.

## Reading material
Some books on the subject of multi-threaded programming I found quite instructive:

* _Programming with POSIX Threads_ by David R. Butenhof.
* _The Art of Multiprocessor Programming_ by Maurice Herlihy, Nir Shavit.

---

Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

[1]: https://max0x7ba.github.io/atomic_queue/html/benchmarks.html
[2]: https://www.kernel.org/doc/html/latest/scheduler/sched-rt-group.html
[3]: https://en.cppreference.com/w/cpp/atomic/atomic
[4]: https://repositum.tuwien.ac.at/obvutwhs/download/pdf/2582190?originalFilename=true
[5]: https://www.realworldtech.com/forum/?threadid=189711&curpostid=189752
[6]: https://en.wikipedia.org/wiki/X86_debug_register#DR7_-_Debug_control
[7]: https://man7.org/linux/man-pages/man2/perf_event_open.2.html
[8]: https://github.com/torvalds/linux/blob/master/include/uapi/linux/hw_breakpoint.h
[9]: https://stackoverflow.com/a/25168942/412080
[10]: https://en.cppreference.com/w/cpp/atomic/atomic/is_lock_free
[11]: https://en.cppreference.com/w/cpp/language/type
[13]: https://en.cppreference.com/w/cpp/error/assert
[14]: https://github.com/max0x7ba/thp-usage
[15]: https://www.kernel.org/doc/html/latest/admin-guide/mm/transhuge.html
[16]: https://en.cppreference.com/w/cpp/atomic/atomic/is_always_lock_free
[17]: https://en.cppreference.com/w/cpp/atomic/memory_order
[18]: https://github.com/max0x7ba/atomic_queue/blob/master/.github/workflows
[19]: https://man7.org/linux/man-pages/man2/sched_yield.2.html
[20]: https://en.wikipedia.org/wiki/Goodhart%27s_law

[^1]: A security feature that cripples CPU performance to protect against someone else's threat vectors. Always disabled in my workstations.
[^2]: Always enabled in my workstations for best performance in memory/compute-intensive workloads, such as linear algebra computations with `numpy` and `Pandas`, training ANNs on GPUs with `PyTorch`.
[^3]: "Completely Fair Scheduler" was never a good idea nor a right solution for anything: `make -j` freezes a Linux system because the process scheduler strives to allocate the CPU/memory resources to all processes equally fairly. The _scheduler automatic task group creation_ Linux feature, now enabled by default, breaks `nice` and creates more problem trying to put a band-aid on the ill-conceived "Completely Fair Scheduler" idea. Desirable robust process scheduling is the opposite of fair. Solaris implemented the desirable robust process scheduling in 2000s, `make -j` never destabilised Solaris. `make -j` was the right way to build fast on any Unix. Switching to Linux, people got most surprised with `make -j` completely freezing the highest-end Linux systems in 2009, and had to learn that `-j` option takes an integer argument (my experience). GNU Make implemented `-l` band-aid option to work-around this Linux-only CPU scheduler problem, and `-l` is worthless for 99% of use-cases (people still hope it solves 1% of use-cases, which have been elusive, so far). And here we are now, with fundamentally completely broken "Completely Fair Scheduler", with its _scheduler automatic task group creation_ band-aid completely breaking `nice`. And a worthless `make -l` workaround for `make -j` completely destabilising Linux with its "Completely Fair Scheduler". While the deluge of Linux security mitigations severely crippling CPU performance for protection against threat vectors non-existent for 99% of Linux users.
