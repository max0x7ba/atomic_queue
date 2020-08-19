[![C/C++ CI](https://github.com/max0x7ba/atomic_queue/workflows/C/C++%20CI/badge.svg)](https://github.com/max0x7ba/atomic_queue/actions?query=workflow%3A%22C%2FC%2B%2B+CI%22)

# atomic_queue
C++14 multiple-producer-multiple-consumer *lockless* queues based on circular buffer with [`std::atomic`][3].

The main design principle these queues follow is _simplicity_: the bare minimum of atomic operations, fixed size buffer, value semantics.

The circular buffer side-steps the memory reclamation problem inherent in linked-list based queues for the price of fixed buffer size. See [Effective memory reclamation for lock-free data structures in C++][4] for more details.

These qualities are also limitations:

* The maximum queue size must be set at compile time or construction time.
* There are no OS-blocking push/pop functions.

Nevertheless, ultra-low-latency applications need just that and nothing more. The simplicity pays off, see the [throughput and latency benchmarks][1].

Available containers are:
* `AtomicQueue` - a fixed size ring-buffer for atomic elements.
* `OptimistAtomicQueue` - a faster fixed size ring-buffer for atomic elements which busy-waits when empty or full.
* `AtomicQueue2` - a fixed size ring-buffer for non-atomic elements.
* `OptimistAtomicQueue2` - a faster fixed size ring-buffer for non-atomic elements which busy-waits when empty or full.

These containers have corresponding `AtomicQueueB`, `OptimistAtomicQueueB`, `AtomicQueueB2`, `OptimistAtomicQueueB2` versions where the buffer size is specified as an argument to the constructor.

Totally ordered mode is supported. In this mode consumers receive messages in the same FIFO order the messages were posted. This mode is supported for `push` and `pop` functions, but for not the `try_` versions. On Intel x86 the totally ordered mode has 0 cost, as of 2019.

Single-producer-single-consumer mode is supported. In this mode, no read-modify-write instructions are necessary, only the atomic loads and stores. That improves queue throughput significantly.

A few other thread-safe containers are used for reference in the benchmarks:
* `std::mutex` - a fixed size ring-buffer with `std::mutex`.
* `pthread_spinlock` - a fixed size ring-buffer with `pthread_spinlock_t`.
* `boost::lockfree::spsc_queue` - a wait-free single-producer-single-consumer queue from Boost library.
* `boost::lockfree::queue` - a lock-free multiple-producer-multiple-consumer queue from Boost library.
* `moodycamel::ConcurrentQueue` - a lock-free multiple-producer-multiple-consumer queue used in non-blocking mode.
* `moodycamel::ReaderWriterQueue` - a lock-free single-producer-single-consumer queue used in non-blocking mode.
* `xenium::michael_scott_queue` - a lock-free multi-producer-multi-consumer queue proposed by [Michael and Scott](http://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf) (this queue is similar to `boost::lockfree::queue` which is also based on the same proposal).
* `xenium::ramalhete_queue` - a lock-free multi-producer-multi-consumer queue proposed by [Ramalhete and Correia](http://concurrencyfreaks.blogspot.com/2016/11/faaarrayqueue-mpmc-lock-free-queue-part.html).
* `xenium::vyukov_bounded_queue` - a bounded multi-producer-multi-consumer queue based on the version proposed by [Vyukov](https://groups.google.com/forum/#!topic/lock-free/-bqYlfbQmH0).
* `tbb::spin_mutex` - a locked fixed size ring-buffer with `tbb::spin_mutex` from Intel Threading Building Blocks.
* `tbb::concurrent_bounded_queue` - eponymous queue used in non-blocking mode from Intel Threading Building Blocks.

# Using the library
The containers provided are header-only class templates, no building/installing is necessary.

1. Clone the project:
```
git clone https://github.com/max0x7ba/atomic_queue.git
```
2. Add `atomic_queue/include` directory (use full path) to the include paths of your build system.
3. `#include <atomic_queue/atomic_queue.h>` in your C++ source.


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

# API
The containers support the following APIs:
* `try_push` - Appends an element to the end of the queue. Returns `false` when the queue is full.
* `try_pop` - Removes an element from the front of the queue. Returns `false` when the queue is empty.
* `push` - Appends an element to the end of the queue. Busy waits when the queue is full. Faster than `try_push` when the queue is not full. Optional FIFO producer queuing and total order.
* `pop` - Removes an element from the front of the queue. Busy waits when the queue is empty. Faster than `try_pop` when the queue is not empty. Optional FIFO consumer queuing and total order.
* `was_size` - Returns the number of unconsumed elements during the call. The state may have changed by the time the return value is examined.
* `was_empty` - Returns `true` if the container was empty during the call. The state may have changed by the time the return value is examined.
* `was_full` - Returns `true` if the container was full during the call. The state may have changed by the time the return value is examined.
* `capacity` - Returns the maximum number of elements the queue can possibly hold.

See [example.cc](src/example.cc) for a usage example.

TODO: full API reference.

# Implementation Notes
The available queues here use a ring-buffer array for storing elements. The size of the queue is fixed at compile time or construction time.

In a production multiple-producer-multiple-consumer scenario the ring-buffer size should be set to the maximum allowable queue size. When the buffer size is exhausted it means that the consumers cannot consume the elements fast enough, fixing which would require either of:

* increasing the buffer size to be able to handle temporary spikes of produced elements, or
* increasing the number of consumers to consume elements faster, or
* decreasing the number of producers to producer fewer elements.

Using a power-of-2 ring-buffer array size allows a couple of important optimizations:

* The writer and reader indexes get mapped into the ring-buffer array index using modulo `% SIZE` binary operator and using a power-of-2 size turns that modulo operator into one plain `and` instruction and that is as fast as it gets.
* The *element index within the cache line* gets swapped with the *cache line index* within the *ring-buffer array element index*, so that subsequent queue elements actually reside in different cache lines. This eliminates contention between producers and consumers on the ring-buffer cache lines. Instead of `N` producers together with `M` consumers competing on the same ring-buffer array cache line in the worst case, it is only one producer competing with one consumer. This optimisation scales better with the number of producers and consumers, and element size. With low number of producers and consumers (up to about 2 of each in these benchmarks) disabling this optimisation may yield better throughput (but higher variance across runs).

The containers use `unsigned` type for size and internal indexes. On x86-64 platform `unsigned` is 32-bit wide, whereas `size_t` is 64-bit wide. 64-bit instructions utilise an extra byte instruction prefix resulting in slightly more pressure on the CPU instruction cache and the front-end. Hence, 32-bit `unsigned` indexes are used to maximise performance. That limits the queue size to 4,294,967,295 elements, which seems to be a reasonable hard limit for many applications.

While the atomic queues can be used with any moveable element types (including `std::unique_ptr`), for best througput and latency the queue elements should be cheap to copy and lock-free (e.g. `unsigned` or `T*`), so that `push` and `pop` operations complete fastest.

`push` and `pop` both perform two atomic operations: increment the counter to claim the element slot and store the element into the array. If a thread calling `push` or `pop` is pre-empted between the two atomic operations that causes another thread calling `pop` or `push` (corresondingly) on the same slot to spin on loading the element until the element is stored; other threads calling `push` and `pop` are not affected. Using real-time `SCHED_FIFO` threads reduces the risk of pre-emption, however, a higher priority `SCHED_FIFO` thread or kernel interrupt handler can still preempt your `SCHED_FIFO` thread. If the queues are used on isolated cores with real-time priority threads, in which case no pre-emption or interrupts occur, the queues operations become _lock-free_.

So, ideally, you may like to run your critical low-latency code on isolated cores that also no other processes can possibly use. And disable [real-time thread throttling](#real-time-thread-throttling) to prevent `SCHED_FIFO` real-time threads from being throttled.

Some people proposed busy-waiting with a call to `sched_yield`/`pthread_yield`. However, `sched_yield` is a wrong tool for locking because it doesn't communicate to the OS kernel what the thread is waiting for, so that the OS scheduler can never wake up the calling thread at the "right" time, unless there are no other threads that can run on this CPU. [More details about `sched_yield` and spinlocks from Linus Torvalds](https://www.realworldtech.com/forum/?threadid=189711&curpostid=189752).

# Benchmarks
[View throughput and latency benchmarks charts][1].

## Methodology
There are a few OS behaviours that complicate benchmarking:
* CPU scheduler can place threads on different CPU cores each run. To avoid that the threads are pinned to specific CPU cores.
* CPU scheduler can preempt threads. To avoid that real-time `SCHED_FIFO` priority 50 is used to disable scheduler time quantum expiry and make the threads non-preemptable by lower priority processes/threads.
* Real-time thread throttling disabled.
* Adverse address space randomisation may cause extra CPU cache conflicts. To minimise effects of that `benchmarks` executable is run at least 33 times and then the results with the highest throughput / lowest latency are selected.

I only have access to a few x86-64 machines. If you have access to different hardware feel free to submit the output file of `scripts/run-benchmarks.sh` and I will include your results into the benchmarks page.

### Huge pages
When huge pages are available the benchmarks use 1x1GB or 16x2MB huge pages for the queues to minimise TLB misses. To enable huge pages do one of:
```
sudo hugeadm --pool-pages-min 1GB:1 --pool-pages-max 1GB:1
sudo hugeadm --pool-pages-min 2MB:16 --pool-pages-max 2MB:16
```

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
The project uses `.editorconfig` and `.clang-format` to automate formatting. Pull requests are expected to be formatted using these settings.

## Help needed
* Submit pull requests with benchmarking code for other queues. The queues should be somewhat widely used or have exceptional performance, not my-first-mpmc-queue type projects.
* Benchmarking results on different architectures or with much more cores. Run `scripts/run-benchmarks.sh` and email me the results file, or put it under `results/` and submit a pull request.

---

Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

[1]: https://max0x7ba.github.io/atomic_queue/html/benchmarks.html
[2]: https://www.kernel.org/doc/html/latest/scheduler/sched-rt-group.html
[3]: https://en.cppreference.com/w/cpp/header/atomic
[4]: https://repositum.tuwien.ac.at/obvutwhs/download/pdf/2582190?originalFilename=true
