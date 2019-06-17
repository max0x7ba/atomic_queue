# atomic_queue
Multiple producer multiple consumer C++14 *lock-free* queues based on `std::atomic<>`. The queues employ busy loops, so they are not *wait-free*.

The main idea these queues utilize is _simplicity_: fixed size buffer, busy wait.

These qualities are also limitations: the maximum queue size must be set at compile time, there are no blocking push/pop functionality. Nevertheless, ultra-low-latency applications need just that and nothing more. The simplicity pays off (see the [throughput and latency benchmarks][1]).

Available containers are:
* `AtomicQueue` - a fixed size ring-buffer for atomic elements.
* `OptimistAtomicQueue` - a faster fixed size ring-buffer for atomic elements which busy-waits when empty or full.
* `AtomicQueue2` - a fixed size ring-buffer for non-atomic elements.
* `OptimistAtomicQueue2` - a faster fixed size ring-buffer for non-atomic elements which busy-waits when empty or full.

A few well-known containers are used for reference in the benchmarks:
* `boost::lockfree::spsc_queue` - a wait-free single producer single consumer queue from Boost library.
* `boost::lockfree::queue` - a lock-free multiple producer multiple consumer queue from Boost library.
* `pthread_spinlock` - a locked fixed size ring-buffer with `pthread_spinlock_t`.
* `moodycamel::ConcurrentQueue` - a lock-free multiple producer multiple consumer queue used in non-blocking mode.
* `tbb::spin_mutex` - a locked fixed size ring-buffer with `tbb::spin_mutex` from Intel Threading Building Blocks.
* `tbb::speculative_spin_mutex` - a locked fixed size ring-buffer with `tbb::speculative_spin_mutex` from Intel Threading Building Blocks.
* `tbb::concurrent_bounded_queue` - eponymous queue used in non-blocking mode from Intel Threading Building Blocks.

# Build and run instructions
The containers provided are header-only class templates that require only `#include <atomic_queue/atomic_queue.h>`, no building/installing is necessary.

Building is neccessary to run the tests and benchmarks.

```
git clone https://github.com/cameron314/concurrentqueue.git
git clone https://github.com/max0x7ba/atomic_queue.git
cd atomic_queue
make -r -j4 run_tests
make -r -j4 run_benchmarks
```

The benchmark also requires Intel TBB library to be available. It assumes that it is installed in `/usr/local/include` and `/usr/local/lib`. If it is installed elsewhere you may like to modify `cppflags.tbb` and `ldlibs.tbb` in `Makefile`.

# API
The containers support the following APIs:
* `try_push` - Appends an element to the end of the queue. Returns `false` when the queue is full.
* `try_pop` - Removes an element from the front of the queue. Returns `false` when the queue is empty.
* `push` - Appends an element to the end of the queue. Busy waits when the queue is full. Faster than `try_push` when the queue is not full.
* `pop` - Removes an element from the front of the queue. Busy waits when the queue is empty. Faster than `try_pop` when the queue is not empty.
* `was_empty` - Returns `true` if the container was empty during the call. The state may have changed by the time the return value is examined.
* `was_full` - Returns `true` if the container was full during the call. The state may have changed by the time the return value is examined.

# Notes
The available queues here use a ring-buffer array for storing elements. The size of the queue is fixed at compile time.

In a real-world multiple-producer-multiple-consumer scenario the ring-buffer size should be set to the maximum allowable queue size. When the buffer size is exhausted it means that the consumers cannot consume the elements fast enough, fixing which would require either of:

* increasing the buffer size to be able to handle temporary spikes of produced elements, or
* increasing the number of consumers to consume elements faster, or
* decreasing the number of producers to producer fewer elements.

Using a power-of-2 ring-buffer array size allows a couple of optimizations:

* The writer and reader indexes get mapped into the ring-buffer array index using modulo `% SIZE` binary operator and using a power-of-2 size turns that modulo operator into one plain `and` instruction and that is as fast as it gets.
* The *element index within the cache line* gets swapped with the *cache line index* within the *ring-buffer array element index*, so that subsequent queue elements actually reside in different cache lines. This eliminates contention between producers and consumers on the ring-buffer cache lines. Instead of N producers together with M consumers competing on the same ring-buffer array cache line in the worst case, it is only one producer competing with one consumer.

In other words, power-of-2 ring-buffer array size yields top performance.

# Benchmarks
I have access to x86-64 hardware only. If you use a different architecture you may like to run tests a few times first and make sure that they pass. If they don't you may like to raise an issue.

[View throughput and latency benchmarks charts][1].

## Ping-pong benchmark
One thread posts an integer to another thread and waits for the reply using two queues. The benchmarks measures the total time of 100,000 ping-pongs, best of 10 runs. Contention is minimal here to be able to achieve and measure the lowest latency. Reports the average round-trip time.

## Throughput and scalability benchmark
N producer threads push a 4-byte integer into one queue, N consumer threads pop the integers from the queue. All producers posts 1,000,000 messages in total. Total time to send and receive all the messages is measured. The benchmark is run for from 1 producer and 1 consumer up to `(total-number-of-cpus / 2)` producers/consumers to measure the scalabilty of different queues.

(C) Maxim Egorushkin 2019

[1]: https://max0x7ba.github.io/atomic_queue/html/benchmarks.html
