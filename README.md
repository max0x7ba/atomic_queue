# atomic_queue
Multiple producer multiple consumer C++ *lock-free* queues. They contain busy loops, so they are not *wait-free*.
Available containers are:
* `AtomicQueue` - a fixed size ring-buffer for atomic elements.
* `BlockingAtomicQueue` - a faster fixed size ring-buffer for atomic elements which busy-waits when empty or full.
* `AtomicQueue2` - a fixed size ring-buffer for non-atomic elements.
* `BlockingAtomicQueue2` - a faster fixed size ring-buffer for non-atomic elements which busy-waits when empty or full.
* `pthread_spinlock` - a fixed size ring-buffer for non-atomic elements, uses `pthread_spinlock_t` for locking.
* `SpinlockHle` - a fixed size ring-buffer for non-atomic elements, uses a spinlock with Intel Hardware Lock Elision (only when compiling with gcc).

# Notes

In a real-world multiple-producer-multiple-consumer scenario the ring-buffer size should be set to the maximum allowable queue size. When the buffer size is exacted it means that the consumers cannot consume the elements fast enough, fixing which would require either of:

* increasing the buffer size to be able to handle spikes of produced elements, or
* increasing the number of consumers, or
* decreasing the number of producers.

All the available queues here use a ring-buffer array for storing queue elements.

Using a power-of-2 ring-buffer array size allows for a couple of important optimizations:

* The writer and reader indexes get mapped into the ring-buffer array index using modulo `% SIZE` binary operator (division and modulo are some of the most expensive operations). The array size `SIZE` is fixed at the compile time, so that the compiler may be able to turn the modulo operator into an assembly block of less expensive instructions. However, a power-of-2 size turns that modulo operator into one plain `and` instruction and that is as fast as it gets.
* The *element index within the cache line* gets swapped with the *cache line index* within the *ring-buffer array element index*, so that logically subsequent elements reside in different cache lines. This eliminates contention between producers and consumers on the ring-buffer cache lines. Instead of N producers together with M consumers competing on the same ring-buffer array cache line in the worst case, it is only one producer competing with one consumer.

In other words, power-of-2 ring-buffer array size yields top performance.

# API
The containers support the following APIs:
* `try_push` - Appends an element to the end of the queue. Returns `false` when the queue is full.
* `try_pop` - Removes an element from the front of the queue. Returns `false` when the queue is empty.
* `push` - Appends an element to the end of the queue. Busy waits when the queue is full. Faster than `try_push` when the queue is not full.
* `pop` - Removes an element from the front of the queue. Busy waits when the queue is empty. Faster than `try_pop` when the queue is not empty.
* `was_empty` - Returns `true` if the container was empty during the call. The state may have changed by the time the return value is examined.
* `was_full` - Returns `true` if the container was full during the call. The state may have changed by the time the return value is examined.

# Build and run instructions
```
git clone git@github.com:max0x7ba/atomic_queue.git
cd atomic_queue
make -r -j4 run_tests
make -r -j4 run_benchmarks
```

# Benchmarks
I have access to x86-64 hardware only. If you use a different architecture run tests a dozen times first and see if they pass. If they don't file an issue.

## Latency and throughput benchmark
Two producer threads post into one queue, two consumer threads drain the queue. Producer and consumer total times are measured. Producer latencies are times it takes to post one item. Consumer latencies are the time it took the item to arrive. These times include the price of rdtsc instruction.

Results on Intel Core i7-7700K, Ubuntu 18.04.2 LTS:
```
         AtomicQueue:  45,693,411 msg/sec.
 BlockingAtomicQueue: 105,864,833 msg/sec.
        AtomicQueue2:  46,758,973 msg/sec.
BlockingAtomicQueue2:  88,649,348 msg/sec.
    pthread_spinlock:  38,080,075 msg/sec.
         SpinlockHle:  19,616,090 msg/sec.
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
         AtomicQueue:  11,918,525 msg/sec.
 BlockingAtomicQueue:  30,782,880 msg/sec.
        AtomicQueue2:  12,799,875 msg/sec.
BlockingAtomicQueue2:  27,159,887 msg/sec.
    pthread_spinlock:  10,780,538 msg/sec.
         SpinlockHle:   5,214,240 msg/sec.
```
## Ping-pong benchmark
One thread posts an integer to another thread and waits for the reply using two queues. The benchmarks measures the total time of 1,000,000 ping-pongs, best of 10 runs. Contention is minimal here to be able to achieve and measure the lowest latency. Reports the total time and the average round-trip time. Wait-free `boost::lockfree::spsc_queue` and a pthread_spinlock-based queue are used as reference benchmarks.

Results on Intel Core i7-7700K, Ubuntu 18.04.2 LTS:
```
   boost::spsc_queue: 0.000000114 sec/round-trip.
         AtomicQueue: 0.000000161 sec/round-trip.
 BlockingAtomicQueue: 0.000000092 sec/round-trip.
        AtomicQueue2: 0.000000190 sec/round-trip.
BlockingAtomicQueue2: 0.000000157 sec/round-trip.
    pthread_spinlock: 0.000000309 sec/round-trip.
         SpinlockHle: 0.000000245 sec/round-trip.
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
   boost::spsc_queue: 0.000000249 sec/round-trip.
         AtomicQueue: 0.000000360 sec/round-trip.
 BlockingAtomicQueue: 0.000000216 sec/round-trip.
        AtomicQueue2: 0.000000421 sec/round-trip.
BlockingAtomicQueue2: 0.000000308 sec/round-trip.
    pthread_spinlock: 0.000000723 sec/round-trip.
         SpinlockHle: 0.000000266 sec/round-trip.
```

# TODO
* CMake.
* More benchmarks.
* More Documentation.
