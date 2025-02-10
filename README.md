[![C++14](https://img.shields.io/badge/dialect-C%2B%2B14-blue)](https://en.cppreference.com/w/cpp/14)
[![MIT license](https://img.shields.io/github/license/max0x7ba/atomic_queue)](https://github.com/max0x7ba/atomic_queue/blob/master/LICENSE)
![Latest release](https://img.shields.io/github/v/tag/max0x7ba/atomic_queue?label=latest%20release)
<br>
[![Conan Center](https://img.shields.io/conan/v/atomic_queue)](https://conan.io/center/recipes/atomic_queue)
![Vcpkg Version](https://img.shields.io/vcpkg/v/atomic-queue)
<br>
[![Makefile Continuous Integrations](https://github.com/max0x7ba/atomic_queue/actions/workflows/ci.yml/badge.svg)](https://github.com/max0x7ba/atomic_queue/actions/workflows/ci.yml)
[![CMake Continuous Integrations](https://github.com/max0x7ba/atomic_queue/actions/workflows/cmake-gcc-clang.yml/badge.svg)](https://github.com/max0x7ba/atomic_queue/actions/workflows/cmake-gcc-clang.yml)
<br>
![platform Linux x86_64](https://img.shields.io/badge/platform-Linux%20x86_64--bit-yellow)
![platform Linux ARM](https://img.shields.io/badge/platform-Linux%20ARM-yellow)
![platform Linux RISC-V](https://img.shields.io/badge/platform-Linux%20RISC--V-yellow)
![platform Linux PowerPC](https://img.shields.io/badge/platform-Linux%20PowerPC-yellow)
![platform Linux IBM System/390](https://img.shields.io/badge/platform-Linux%20IBM%20System/390-yellow)

# atomic_queue
C++14 multiple-producer-multiple-consumer *lock-free* queues based on circular buffer and [`std::atomic`][3].

Designed with a goal to minimize the latency between one thread pushing an element into a queue and another thread popping it from the queue.

It has been developed, tested and benchmarked on Linux, but should support any C++14 platforms which implement `std::atomic`. Reported as compatible with Windows, but the continuous integrations hosted by GitHub are currently set up only for x86_64 platform on Ubuntu-20.04 and Ubuntu-22.04. Pull requests to extend the [continuous integrations][18] to run on other architectures and/or platforms are welcome.

## Design Principles
When minimizing latency a good design is not when there is nothing left to add, but rather when there is nothing left to remove, as these queues exemplify.

Minimizing latency naturally maximizes throughput. Low latency reciprocal is high throuhput, in ideal mathematical and practical engineering sense. Low latency is incompatible with any delays and/or batching, which destroy original (hardware) global time order of events pushed into one queue by different threads. Maximizing throughput, on the other hand, can be done at expense of latency by delaying and batching multiple updates.

The main design principle these queues follow is _minimalism_, which results in such design choices as:

* Bare minimum of atomic instructions. Inlinable by default push and pop functions can hardly be any cheaper in terms of CPU instruction number / L1i cache pressure.
* Explicit contention/false-sharing avoidance for queue and its elements.
* Linear fixed size ring-buffer array. No heap memory allocations after a queue object has constructed. It doesn't get any more CPU L1d or TLB cache friendly than that.
* Value semantics. Meaning that the queues make a copy/move upon `push`/`pop`, no reference/pointer to elements in the queue can be obtained.

The impact of each of these small design choices on their own is barely measurable, but their total impact is much greater than a simple sum of the constituents' impacts, aka super-scalar compounding or synergy. The synergy emerging from combining multiple of these small design choices together is what allows CPUs to perform at their peak capacities least impeded.

These design choices are also limitations:

* The maximum queue size must be set at compile time or construction time. The circular buffer side-steps the memory reclamation problem inherent in linked-list based queues for the price of fixed buffer size. See [Effective memory reclamation for lock-free data structures in C++][4] for more details. Fixed buffer size may not be that much of a limitation, since once the queue gets larger than the maximum expected size that indicates a problem that elements aren't consumed fast enough, and if the queue keeps growing it may eventually consume all available memory which may affect the entire system, rather than the problematic process only. The only apparent inconvenience is that one has to do an upfront calculation on what would be the largest expected/acceptable number of unconsumed elements in the queue.
* There are no OS-blocking push/pop functions. This queue is designed for ultra-low-latency scenarios and using an OS blocking primitive would be sacrificing push-to-pop latency. For lowest possible latency one cannot afford blocking in the OS kernel because the wake-up latency of a blocked thread is about 1-3 microseconds, whereas this queue's round-trip time can be as low as 150 nanoseconds.

Ultra-low-latency applications need just that and nothing more. The minimalism pays off, see the [throughput and latency benchmarks][1].

## Role Models
Several other well established and popular thread-safe containers are used for reference in the [benchmarks][1]:
* `std::mutex` - a fixed size ring-buffer with `std::mutex`.
* `pthread_spinlock` - a fixed size ring-buffer with `pthread_spinlock_t`.
* `boost::lockfree::spsc_queue` - a wait-free single-producer-single-consumer queue from Boost library.
* `boost::lockfree::queue` - a lock-free multiple-producer-multiple-consumer queue from Boost library.
* `moodycamel::ConcurrentQueue` - a lock-free multiple-producer-multiple-consumer queue used in non-blocking mode. This queue is designed to maximize throughput at the expense of latency and eschewing the global time order of elements pushed into one queue by different threads. It is not equivalent to other queues benchmarked here in this respect.
* `moodycamel::ReaderWriterQueue` - a lock-free single-producer-single-consumer queue used in non-blocking mode.
* `xenium::michael_scott_queue` - a lock-free multi-producer-multi-consumer queue proposed by [Michael and Scott](http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf) (this queue is similar to `boost::lockfree::queue` which is also based on the same proposal).
* `xenium::ramalhete_queue` - a lock-free multi-producer-multi-consumer queue proposed by [Ramalhete and Correia](http://concurrencyfreaks.blogspot.com/2016/11/faaarrayqueue-mpmc-lock-free-queue-part.html).
* `xenium::vyukov_bounded_queue` - a bounded multi-producer-multi-consumer queue based on the version proposed by [Vyukov](https://groups.google.com/forum/#!topic/lock-free/-bqYlfbQmH0).
* `tbb::spin_mutex` - a locked fixed size ring-buffer with `tbb::spin_mutex` from Intel Threading Building Blocks.
* `tbb::concurrent_bounded_queue` - eponymous queue used in non-blocking mode from Intel Threading Building Blocks.

# Using the library
The containers provided are header-only class templates, no building/installing is necessary.

## Install from GitHub
1. Clone the project:
```
git clone https://github.com/max0x7ba/atomic_queue.git
```
2. Add `atomic_queue/include` directory (use full path) to the include paths of your build system.
3. `#include <atomic_queue/atomic_queue.h>` in your C++ source.

## Install using vcpkg
```
vcpkg install atomic-queue
```

## Install using conan
Follow the official tutorial on [how to consume conan packages](https://docs.conan.io/2/tutorial/consuming_packages.html).
Details specific to this library are available in [ConanCenter](https://conan.io/center/recipes/atomic_queue).

## Benchmark build and run instructions
The containers provided are header-only class templates that require only `#include <atomic_queue/atomic_queue.h>`, no building/installing is necessary.

Building is necessary to run the tests and benchmarks.

```
git clone https://github.com/cameron314/concurrentqueue.git
git clone https://github.com/cameron314/readerwriterqueue.git
git clone https://github.com/mpoeter/xenium.git
git clone https://github.com/max0x7ba/atomic_queue.git
cd atomic_queue
make -r -j4 run_benchmarks
```

The benchmark also requires Intel TBB library to be available. It assumes that it is installed in `/usr/local/include` and `/usr/local/lib`. If it is installed elsewhere you may like to modify `cppflags.tbb` and `ldlibs.tbb` in `Makefile`.

# Library contemts
## Available queues
* `AtomicQueue` - a fixed size ring-buffer for atomic elements.
* `OptimistAtomicQueue` - a faster fixed size ring-buffer for atomic elements which busy-waits when empty or full. It is `AtomicQueue` used with `push`/`pop` instead of `try_push`/`try_pop`.
* `AtomicQueue2` - a fixed size ring-buffer for non-atomic elements.
* `OptimistAtomicQueue2` - a faster fixed size ring-buffer for non-atomic elements which busy-waits when empty or full. It is `AtomicQueue2` used with `push`/`pop` instead of `try_push`/`try_pop`.

These containers have corresponding `AtomicQueueB`, `OptimistAtomicQueueB`, `AtomicQueueB2`, `OptimistAtomicQueueB2` versions where the buffer size is specified as an argument to the constructor.

Totally ordered mode is supported. In this mode consumers receive messages in the same FIFO order the messages were posted. This mode is supported for `push` and `pop` functions, but for not the `try_` versions. On Intel x86 the totally ordered mode has 0 cost, as of 2019.

Single-producer-single-consumer mode is supported. In this mode, no expensive atomic read-modify-write CPU instructions are necessary, only the cheapest atomic loads and stores. That improves queue throughput significantly.

Move-only queue element types are fully supported. For example, a queue of `std::unique_ptr<T>` elements would be `AtomicQueue2B<std::unique_ptr<T>>` or `AtomicQueue2<std::unique_ptr<T>, CAPACITY>`.

## Queue schematics

```
queue-end                 queue-front
[newest-element, ..., oldest-element]
push()                          pop()
```

## Queue API
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

# Implementation Notes
## Memory order of non-atomic loads and stores
`push` and `try_push` operations _synchronize-with_ (as defined in [`std::memory_order`][17]) with any subsequent `pop` or `try_pop` operation of the same queue object. Meaning that:
* No non-atomic load/store gets reordered past `push`/`try_push`, which is a `memory_order::release` operation. Same memory order as that of `std::mutex::unlock`.
* No non-atomic load/store gets reordered prior to `pop`/`try_pop`, which is a `memory_order::acquire` operation. Same memory order as that of `std::mutex::lock`.
* The effects of a producer thread's non-atomic stores followed by `push`/`try_push` of an element into a queue become visible in the consumer's thread which `pop`/`try_pop` that particular element.

## Ring-buffer capacity
The available queues here use a ring-buffer array for storing elements. The capacity of the queue is fixed at compile time or construction time.

In a production multiple-producer-multiple-consumer scenario the ring-buffer capacity should be set to the maximum expected queue size. When the ring-buffer gets full it means that the consumers cannot consume the elements fast enough. A fix for that is any of:

* Increase the queue capacity in order to handle temporary spikes of pending elements in the queue. This normally requires restarting the application after re-configuration/re-compilation has been done.
* Increase the number of consumers to drain the queue faster. The number of consumers can be managed dynamically, e.g.: when a consumer observes that the number of elements pending in the queue keeps growing, that calls for deploying more consumer threads to drain the queue at a faster rate; mostly empty queue calls for suspending/terminating excess consumer threads.
* Decrease the rate of pushing elements into the queue. `push` and `pop` calls always incur some expensive CPU cycles to maintain the integrity of queue state in atomic/consistent/isolated fashion with respect to other threads and these costs increase super-linearly as queue contention grows. Producer batching of multiple small elements or elements resulting from one event into one queue message is often a reasonable solution.

Using a power-of-2 ring-buffer array size allows a couple of important optimizations:

* The writer and reader indexes get mapped into the ring-buffer array index using remainder binary operator `% SIZE`. Remainder binary operator `%` normally generates a division CPU instruction which isn't cheap, but using a power-of-2 size turns that remainder operator into one cheap binary `and` CPU instruction and that is as fast as it gets.
* The *element index within the cache line* gets swapped with the *cache line index*, so that consecutive queue elements reside in different cache lines. This massively reduces cache line contention between multiple producers and multiple consumers. Instead of `N` producers together with `M` consumers competing on subsequent elements in the same ring-buffer cache line in the worst case, it is only one producer competing with one consumer (pedantically, when the number of CPUs is not greater than the number of elements that can fit in one cache line). This optimisation scales better with the number of producers and consumers, and element size. With low number of producers and consumers (up to about 2 of each in these benchmarks) disabling this optimisation may yield better throughput (but higher variance across runs).

The containers use `unsigned` type for size and internal indexes. On x86-64 platform `unsigned` is 32-bit wide, whereas `size_t` is 64-bit wide. 64-bit instructions utilise an extra byte instruction prefix resulting in slightly more pressure on the CPU instruction cache and the front-end. Hence, 32-bit `unsigned` indexes are used to maximise performance. That limits the queue size to 4,294,967,295 elements, which seems to be a reasonable hard limit for many applications.

While the atomic queues can be used with any moveable element types (including `std::unique_ptr`), for best throughput and latency the queue elements should be cheap to copy and lock-free (e.g. `unsigned` or `T*`), so that `push` and `pop` operations complete fastest.

## Lock-free guarantees
*Conceptually*, a `push` or `pop` operation does two atomic steps:

1. Atomically and exclusively claims the queue slot index to store/load an element to/from. That's producers incrementing `head` index, consumers incrementing `tail` index. Each slot is accessed by one producer and one consumer threads only.
2. Atomically store/load the element into/from the slot. Producer storing into a slot changes its state to be non-`NIL`, consumer loading from a slot changes its state to be `NIL`. The slot is a spinlock for its one producer and one consumer threads.

These queues anticipate that a thread doing `push` or `pop` may complete step 1 and then be preempted before completing step 2.

An algorithm is *lock-free* if there is guaranteed system-wide progress. These queue guarantee system-wide progress by the following properties:

* Each `push` is independent of any preceding `push`. An incomplete (preempted) `push` by one producer thread doesn't affect `push` of any other thread.
* Each `pop` is independent of any preceding `pop`. An incomplete (preempted) `pop` by one consumer thread doesn't affect `pop` of any other thread.
* An incomplete (preempted) `push` from one producer thread affects only one consumer thread `pop`ing an element from this particular queue slot. All other threads `pop`s are unaffected.
* An incomplete (preempted) `pop` from one consumer thread affects only one producer thread `push`ing an element into this particular queue slot while expecting it to have been consumed long time ago, in the rather unlikely scenario that producers have wrapped around the entire ring-buffer while this consumer hasn't completed its `pop`. All other threads `push`s and `pop`s are unaffected.

## Preemption
Linux task scheduler thread preemption is something no user-space process should be able to affect or escape, otherwise any/every malicious application would exploit that.

Still, there are a few things one can do to minimize preemption of one's mission critical application threads:

* Use real-time `SCHED_FIFO` scheduling class for your threads, e.g. `chrt --fifo 50 <app>`. A higher priority `SCHED_FIFO` thread or kernel interrupt handler can still preempt your `SCHED_FIFO` threads.
* Use one same fixed real-time scheduling priority for all threads accessing same queue objects. Real-time threads with different scheduling priorities modifying one queue object may cause priority inversion and deadlocks. Using the default scheduling class `SCHED_OTHER` with its dynamically adjusted priorities defeats the purpose of using these queues.
* Disable [real-time thread throttling](#real-time-thread-throttling) to prevent `SCHED_FIFO` real-time threads from being throttled.
* Isolate CPU cores, so that no interrupt handlers or applications ever run on it. Mission critical applications should be explicitly placed on these isolated cores with `taskset`.
* Pin threads to specific cores, otherwise the task scheduler keeps moving threads to other idle CPU cores to level voltage/heat-induced wear-and-tear accross CPU cores. Keeping a thread running on one same CPU core maximizes CPU cache hit rate. Moving a thread to another CPU core incurs otherwise unnecessary CPU cache thrashing.

People often propose limiting busy-waiting with a subsequent call to `std::this_thread::yield()`/`sched_yield`/`pthread_yield`. However, `sched_yield` is a wrong tool for locking because it doesn't communicate to the OS kernel what the thread is waiting for, so that the OS thread scheduler can never schedule the calling thread to resume at the right time when the shared state has changed (unless there are no other threads that can run on this CPU core, so that the caller resumes immediately). See notes section in [`man sched_yield`][19] and [a Linux kernel thread about `sched_yield` and spinlocks][5] for more details.

[In Linux, there is mutex type `PTHREAD_MUTEX_ADAPTIVE_NP`][9] which busy-waits a locked mutex for a number of iterations and then makes a blocking syscall into the kernel to deschedule the waiting thread. In the benchmarks it was the worst performer and I couldn't find a way to make it perform better, and that's the reason it is not included in the benchmarks.

On Intel CPUs one could use [the 4 debug control registers][6] to monitor the spinlock memory region for write access and wait on it using `select` (and its friends) or `sigwait` (see [`perf_event_open`][7] and [`uapi/linux/hw_breakpoint.h`][8] for more details). A spinlock waiter could suspend itself with `select` or `sigwait` until the spinlock state has been updated. But there are only 4 of these registers, so that such a solution wouldn't scale.

# Benchmarks
[View throughput and latency benchmarks charts][1].

## Methodology
There are a few OS behaviours that complicate benchmarking:
* CPU scheduler can place threads on different CPU cores each run. To avoid that the threads are pinned to specific CPU cores.
* CPU scheduler can preempt threads. To avoid that real-time `SCHED_FIFO` priority 50 is used to disable scheduler time quantum expiry and make the threads non-preemptable by lower priority processes/threads.
* Real-time thread throttling disabled.
* Adverse address space randomisation may cause extra CPU cache conflicts, as well as other processes running on the system. To minimise effects of that `benchmarks` executable is run at least 33 times. The benchmark charts display average values. The chart tooltip also displays the standard deviation, minimum and maximum values.

Benchmark performance of single-producer-single-consumer queues `boost::lockfree::spsc_queue`, `moodycamel::ReaderWriterQueue` and these queues in single-producer-single-consumer mode should be identical because they implement exactly the same algorithm using exactly the same atomic load and store instructions. `boost::lockfree::spsc_queue` implementation benchmarked at that time had no optimizations for minimizing L1d cache contention, cold branch misprediction or pipeline stalls from subtler issues noticable only in the generated assembly code.

I only have access to a few x86-64 machines. If you have access to different hardware feel free to submit the output file of `scripts/run-benchmarks.sh` and I will include your results into the benchmarks page.

### Huge pages
When huge pages are available the benchmarks use 1x1GB or 16x2MB huge pages for the queues to minimise TLB misses. To enable huge pages do one of:
```
sudo hugeadm --pool-pages-min 1GB:1
sudo hugeadm --pool-pages-min 2MB:16
```
Alternatively, you may like to enable [transparent hugepages][15] in your system and use a hugepage-aware allocator, such as [tcmalloc][14].

### Real-time thread throttling
By default, Linux scheduler throttles real-time threads from consuming 100% of CPU and that is detrimental to benchmarking. Full details can be found in [Real-Time group scheduling][2]. To disable real-time thread throttling do:
```
echo -1 | sudo tee /proc/sys/kernel/sched_rt_runtime_us >/dev/null
```

## Throughput and scalability benchmark
N producer threads push a 4-byte integer into one same queue, N consumer threads pop the integers from the queue. All producers posts 1,000,000 messages in total. Total time to send and receive all the messages is measured. The benchmark is run for from 1 producer and 1 consumer up to `(total-number-of-cpus / 2)` producers/consumers to measure the scalability of different queues.

## Ping-pong benchmark
One thread posts an integer to another thread through one queue and waits for a reply from another queue (2 queues in total). The benchmarks measures the total time of 100,000 ping-pongs, best of 10 runs. Contention is minimal here (1-producer-1-consumer, 1 element in the queue) to be able to achieve and measure the lowest latency. Reports the average round-trip time.

# Contributing
Contributions are more than welcome. `.editorconfig` and `.clang-format` can be used to automatically match code formatting.

# Reading material
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
[12]: https://en.cppreference.com/w/cpp/types/is_arithmetic
[13]: https://en.cppreference.com/w/cpp/error/assert
[14]: https://google.github.io/tcmalloc/temeraire.html
[15]: https://www.kernel.org/doc/html/latest/admin-guide/mm/transhuge.html
[16]: https://en.cppreference.com/w/cpp/atomic/atomic/is_always_lock_free
[17]: https://en.cppreference.com/w/cpp/atomic/memory_order
[18]: https://github.com/max0x7ba/atomic_queue/blob/master/.github/workflows/ci.yml
[19]: https://man7.org/linux/man-pages/man2/sched_yield.2.html
