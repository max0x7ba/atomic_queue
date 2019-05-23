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

## Throughput benchmark
Two producer threads post into one queue, two consumer threads drain the queue. Each producer posts one million messages. Total time to send and receive the messages is measured.

Results on Intel Core i7-7700K, Ubuntu 18.04.2 LTS:
```
      boost::lockfree::queue:   7,711,548 msg/sec
            pthread_spinlock:  15,790,499 msg/sec
                 AtomicQueue:  15,412,546 msg/sec
         BlockingAtomicQueue:  48,855,955 msg/sec
                AtomicQueue2:  14,992,677 msg/sec
        BlockingAtomicQueue2:  37,112,587 msg/sec
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
      boost::lockfree::queue:   2,504,207 msg/sec
            pthread_spinlock:   5,295,343 msg/sec
                 AtomicQueue:   5,367,912 msg/sec
         BlockingAtomicQueue:  20,940,375 msg/sec
                AtomicQueue2:   5,885,886 msg/sec
        BlockingAtomicQueue2:  14,483,390 msg/sec
```
## Ping-pong benchmark
One thread posts an integer to another thread and waits for the reply using two queues. The benchmarks measures the total time of 100,000 ping-pongs, best of 10 runs. Contention is minimal here to be able to achieve and measure the lowest latency. Reports the average round-trip time.

Results on Intel Core i7-7700K, Ubuntu 18.04.2 LTS:
```
 boost::lockfree::spsc_queue: 0.000000117 sec/round-trip
      boost::lockfree::queue: 0.000000253 sec/round-trip
            pthread_spinlock: 0.000000272 sec/round-trip
                 AtomicQueue: 0.000000146 sec/round-trip
         BlockingAtomicQueue: 0.000000090 sec/round-trip
                AtomicQueue2: 0.000000174 sec/round-trip
        BlockingAtomicQueue2: 0.000000145 sec/round-trip
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
           boost::spsc_queue: 0.000000249 sec/round-trip.
      boost::lockfree::queue: 0.000000708 sec/round-trip
            pthread_spinlock: 0.000000723 sec/round-trip.
                 AtomicQueue: 0.000000354 sec/round-trip.
         BlockingAtomicQueue: 0.000000216 sec/round-trip.
                AtomicQueue2: 0.000000421 sec/round-trip.
        BlockingAtomicQueue2: 0.000000308 sec/round-trip.
```

## Scalability benchmark
This benchmark starts N producers and N consumers. The total throughput in msg/sec is measured.
Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
---- Running throughput and throughput benchmarks (higher is better) ----
      boost::lockfree::queue, 2 producers:   2,287,130 msg/sec
      boost::lockfree::queue, 3 producers:   2,302,414 msg/sec
      boost::lockfree::queue, 4 producers:   1,744,023 msg/sec
      boost::lockfree::queue, 5 producers:   1,534,171 msg/sec
      boost::lockfree::queue, 6 producers:   1,781,191 msg/sec
            pthread_spinlock, 2 producers:   4,590,751 msg/sec
            pthread_spinlock, 3 producers:   3,436,103 msg/sec
            pthread_spinlock, 4 producers:   2,500,333 msg/sec
            pthread_spinlock, 5 producers:   1,871,897 msg/sec
            pthread_spinlock, 6 producers:   2,087,420 msg/sec
                 AtomicQueue, 2 producers:   4,846,906 msg/sec
                 AtomicQueue, 3 producers:   4,158,126 msg/sec
                 AtomicQueue, 4 producers:   2,836,063 msg/sec
                 AtomicQueue, 5 producers:   2,427,541 msg/sec
                 AtomicQueue, 6 producers:   2,268,458 msg/sec
         BlockingAtomicQueue, 2 producers:  13,163,192 msg/sec
         BlockingAtomicQueue, 3 producers:  14,974,197 msg/sec
         BlockingAtomicQueue, 4 producers:  20,585,013 msg/sec
         BlockingAtomicQueue, 5 producers:  21,295,096 msg/sec
         BlockingAtomicQueue, 6 producers:  21,362,910 msg/sec
                AtomicQueue2, 2 producers:   4,634,256 msg/sec
                AtomicQueue2, 3 producers:   3,208,438 msg/sec
                AtomicQueue2, 4 producers:   3,025,483 msg/sec
                AtomicQueue2, 5 producers:   2,486,477 msg/sec
                AtomicQueue2, 6 producers:   2,606,209 msg/sec
        BlockingAtomicQueue2, 2 producers:   8,328,188 msg/sec
        BlockingAtomicQueue2, 3 producers:  11,387,004 msg/sec
        BlockingAtomicQueue2, 4 producers:  17,074,897 msg/sec
        BlockingAtomicQueue2, 5 producers:  14,688,902 msg/sec
        BlockingAtomicQueue2, 6 producers:  19,876,903 msg/sec
