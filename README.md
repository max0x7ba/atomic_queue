# atomic_queue
Multiple producer multiple consumer C++14 *lock-free* queues. They contain busy loops, so they are not *wait-free*.
Available containers are:
* `AtomicQueue` - a fixed size ring-buffer for atomic elements.
* `BlockingAtomicQueue` - a faster fixed size ring-buffer for atomic elements which busy-waits when empty or full.
* `AtomicQueue2` - a fixed size ring-buffer for non-atomic elements.
* `BlockingAtomicQueue2` - a faster fixed size ring-buffer for non-atomic elements which busy-waits when empty or full.
* `pthread_spinlock` - a fixed size ring-buffer for non-atomic elements, uses `pthread_spinlock_t` for locking.
* `SpinlockHle` - a fixed size ring-buffer for non-atomic elements, uses a spinlock with Intel Hardware Lock Elision (only when compiling with gcc).

# Build and run instructions
The containers provided are header-only class templates that require only `#include <atomic_queue/atomic_queue.h>`, no building/installing is necessary.

Building is neccessary to run the tests and benchmarks.

```
git clone git@github.com:max0x7ba/atomic_queue.git
cd atomic_queue
make -r -j4 run_tests
make -r -j4 run_benchmarks
```

# API
The containers support the following APIs:
* `try_push` - Appends an element to the end of the queue. Returns `false` when the queue is full.
* `try_pop` - Removes an element from the front of the queue. Returns `false` when the queue is empty.
* `push` - Appends an element to the end of the queue. Busy waits when the queue is full. Faster than `try_push` when the queue is not full.
* `pop` - Removes an element from the front of the queue. Busy waits when the queue is empty. Faster than `try_pop` when the queue is not empty.
* `was_empty` - Returns `true` if the container was empty during the call. The state may have changed by the time the return value is examined.
* `was_full` - Returns `true` if the container was full during the call. The state may have changed by the time the return value is examined.

# Notes
In a real-world multiple-producer-multiple-consumer scenario the ring-buffer size should be set to the maximum allowable queue size. When the buffer size is exacted it means that the consumers cannot consume the elements fast enough, fixing which would require either of:

* increasing the buffer size to be able to handle spikes of produced elements, or
* increasing the number of consumers, or
* decreasing the number of producers.

All the available queues here use a ring-buffer array for storing queue elements.

Using a power-of-2 ring-buffer array size allows a couple of optimizations:

* The writer and reader indexes get mapped into the ring-buffer array index using modulo `% SIZE` binary operator and using a power-of-2 size turns that modulo operator into one plain `and` instruction and that is as fast as it gets.
* The *element index within the cache line* gets swapped with the *cache line index* within the *ring-buffer array element index*, so that logically subsequent elements reside in different cache lines. This eliminates contention between producers and consumers on the ring-buffer cache lines. Instead of N producers together with M consumers competing on the same ring-buffer array cache line in the worst case, it is only one producer competing with one consumer.

In other words, power-of-2 ring-buffer array size yields top performance.

# Benchmarks
I have access to x86-64 hardware only. If you use a different architecture run tests and see if they pass. If they don't you may like to raise an issue.

## Throughput benchmark
Two producer threads post into one queue, two consumer threads drain the queue. Each producer posts one million messages. Total time to send and receive the messages is measured.

Results on Intel Core i7-7700K, Ubuntu 18.04.2 LTS:
```
         AtomicQueue:  15,412,546 msg/sec
 BlockingAtomicQueue:  48,855,955 msg/sec
        AtomicQueue2:  14,992,677 msg/sec
BlockingAtomicQueue2:  37,112,587 msg/sec
    pthread_spinlock:  24,177,214 msg/sec
         SpinlockHle:   8,952,720 msg/sec
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
         AtomicQueue:   5,367,912 msg/sec
 BlockingAtomicQueue:  20,940,375 msg/sec
        AtomicQueue2:   5,885,886 msg/sec
BlockingAtomicQueue2:  14,483,390 msg/sec
    pthread_spinlock:   5,295,343 msg/sec
         SpinlockHle:   2,761,216 msg/sec
```
## Ping-pong benchmark
One thread posts an integer to another thread and waits for the reply using two queues. The benchmarks measures the total time of 1,000,000 ping-pongs, best of 10 runs. Contention is minimal here to be able to achieve and measure the lowest latency. Reports the total time and the average round-trip time. Wait-free `boost::lockfree::spsc_queue` and a pthread_spinlock-based queue are used as reference benchmarks.

Results on Intel Core i7-7700K, Ubuntu 18.04.2 LTS:
```
   boost::spsc_queue: 0.000000119 sec/round-trip
         AtomicQueue: 0.000000146 sec/round-trip
 BlockingAtomicQueue: 0.000000090 sec/round-trip
        AtomicQueue2: 0.000000174 sec/round-trip
BlockingAtomicQueue2: 0.000000145 sec/round-trip
    pthread_spinlock: 0.000000272 sec/round-trip
         SpinlockHle: 0.000000187 sec/round-trip
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
   boost::spsc_queue: 0.000000249 sec/round-trip.
         AtomicQueue: 0.000000354 sec/round-trip.
 BlockingAtomicQueue: 0.000000216 sec/round-trip.
        AtomicQueue2: 0.000000421 sec/round-trip.
BlockingAtomicQueue2: 0.000000308 sec/round-trip.
    pthread_spinlock: 0.000000723 sec/round-trip.
         SpinlockHle: 0.000000266 sec/round-trip.
```

## Scalability benchmark
This benchmark starts N producers and N consumers. The total throughput in msg/sec is measured.
Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
---- Running throughput and throughput benchmarks (higher is better) ----
         AtomicQueue, 1 producers:   7,553,638 msg/sec
         AtomicQueue, 2 producers:   4,945,975 msg/sec
         AtomicQueue, 3 producers:   3,857,356 msg/sec
         AtomicQueue, 4 producers:   3,100,753 msg/sec
         AtomicQueue, 5 producers:   2,758,586 msg/sec
         AtomicQueue, 6 producers:   2,319,908 msg/sec
 BlockingAtomicQueue, 1 producers:  53,954,367 msg/sec
 BlockingAtomicQueue, 2 producers:  12,375,789 msg/sec
 BlockingAtomicQueue, 3 producers:  14,403,819 msg/sec
 BlockingAtomicQueue, 4 producers:  16,636,900 msg/sec
 BlockingAtomicQueue, 5 producers:  22,108,188 msg/sec
 BlockingAtomicQueue, 6 producers:  22,080,866 msg/sec
        AtomicQueue2, 1 producers:   7,911,739 msg/sec
        AtomicQueue2, 2 producers:   4,927,648 msg/sec
        AtomicQueue2, 3 producers:   3,534,156 msg/sec
        AtomicQueue2, 4 producers:   2,966,052 msg/sec
        AtomicQueue2, 5 producers:   2,704,933 msg/sec
        AtomicQueue2, 6 producers:   2,346,440 msg/sec
BlockingAtomicQueue2, 1 producers:  10,505,730 msg/sec
BlockingAtomicQueue2, 2 producers:  10,190,579 msg/sec
BlockingAtomicQueue2, 3 producers:  13,050,528 msg/sec
BlockingAtomicQueue2, 4 producers:  17,095,960 msg/sec
BlockingAtomicQueue2, 5 producers:  20,831,507 msg/sec
BlockingAtomicQueue2, 6 producers:  19,963,530 msg/sec
    pthread_spinlock, 1 producers:   7,730,726 msg/sec
    pthread_spinlock, 2 producers:   3,166,042 msg/sec
    pthread_spinlock, 3 producers:   1,663,267 msg/sec
    pthread_spinlock, 4 producers:   1,142,015 msg/sec
    pthread_spinlock, 5 producers:   1,724,822 msg/sec
    pthread_spinlock, 6 producers:   1,030,979 msg/sec
         SpinlockHle, 1 producers:   7,448,553 msg/sec
         SpinlockHle, 2 producers:   2,214,847 msg/sec
         SpinlockHle, 3 producers:   1,534,624 msg/sec
         SpinlockHle, 4 producers:     848,513 msg/sec
         SpinlockHle, 5 producers:     636,505 msg/sec
         SpinlockHle, 6 producers:     483,595 msg/sec
```
