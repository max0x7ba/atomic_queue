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
        boost::lockfree::queue:   9,111,702 msg/sec (mean:   8,319,644 stdev:     498,734)
 tbb::concurrent_bounded_queue:  20,505,918 msg/sec (mean:  18,053,459 stdev:   1,869,244)
              pthread_spinlock:  31,091,423 msg/sec (mean:  16,666,098 stdev:   6,403,520)
                   AtomicQueue:  15,865,858 msg/sec (mean:  14,481,723 stdev:     783,641)
           BlockingAtomicQueue:  37,307,748 msg/sec (mean:  31,693,904 stdev:   3,252,467)
                  AtomicQueue2:  14,996,508 msg/sec (mean:  13,964,554 stdev:     676,846)
          BlockingAtomicQueue2:  32,223,186 msg/sec (mean:  30,796,786 stdev:   1,990,534)
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
        boost::lockfree::queue:   2,398,544 msg/sec (mean:   2,302,336 stdev:      94,692)
 tbb::concurrent_bounded_queue:   6,289,462 msg/sec (mean:   5,477,333 stdev:     416,377)
              pthread_spinlock:   5,429,537 msg/sec (mean:   4,283,142 stdev:     594,738)
                   AtomicQueue:   5,425,324 msg/sec (mean:   4,301,079 stdev:     525,305)
           BlockingAtomicQueue:  17,180,243 msg/sec (mean:  13,367,047 stdev:   2,154,565)
                  AtomicQueue2:   5,277,146 msg/sec (mean:   4,479,122 stdev:     480,400)
          BlockingAtomicQueue2:  13,004,012 msg/sec (mean:  11,188,531 stdev:   1,192,558)
```
## Ping-pong benchmark
One thread posts an integer to another thread and waits for the reply using two queues. The benchmarks measures the total time of 100,000 ping-pongs, best of 10 runs. Contention is minimal here to be able to achieve and measure the lowest latency. Reports the average round-trip time.

Results on Intel Core i7-7700K, Ubuntu 18.04.2 LTS:
```
   boost::lockfree::spsc_queue: 0.000000106 sec/round-trip (mean: 0.000000114 stdev: 0.000000005) sec/round-trip
        boost::lockfree::queue: 0.000000268 sec/round-trip (mean: 0.000000280 stdev: 0.000000010) sec/round-trip
 tbb::concurrent_bounded_queue: 0.000003668 sec/round-trip (mean: 0.000003706 stdev: 0.000000018) sec/round-trip
              pthread_spinlock: 0.000000349 sec/round-trip (mean: 0.000000856 stdev: 0.000001082) sec/round-trip
                   AtomicQueue: 0.000000147 sec/round-trip (mean: 0.000000158 stdev: 0.000000006) sec/round-trip
           BlockingAtomicQueue: 0.000000091 sec/round-trip (mean: 0.000000100 stdev: 0.000000008) sec/round-trip
                  AtomicQueue2: 0.000000174 sec/round-trip (mean: 0.000000182 stdev: 0.000000005) sec/round-trip
          BlockingAtomicQueue2: 0.000000190 sec/round-trip (mean: 0.000000193 stdev: 0.000000002) sec/round-trip
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
   boost::lockfree::spsc_queue: 0.000000324 sec/round-trip (mean: 0.000000366 stdev: 0.000000056) sec/round-trip
        boost::lockfree::queue: 0.000000688 sec/round-trip (mean: 0.000000782 stdev: 0.000000130) sec/round-trip
 tbb::concurrent_bounded_queue: 0.000012759 sec/round-trip (mean: 0.000012934 stdev: 0.000000130) sec/round-trip
              pthread_spinlock: 0.000001129 sec/round-trip (mean: 0.000001355 stdev: 0.000000165) sec/round-trip
                   AtomicQueue: 0.000000546 sec/round-trip (mean: 0.000000608 stdev: 0.000000054) sec/round-trip
           BlockingAtomicQueue: 0.000000320 sec/round-trip (mean: 0.000000348 stdev: 0.000000028) sec/round-trip
                  AtomicQueue2: 0.000000408 sec/round-trip (mean: 0.000000477 stdev: 0.000000037) sec/round-trip
          BlockingAtomicQueue2: 0.000000359 sec/round-trip (mean: 0.000000400 stdev: 0.000000032) sec/round-trip
```

## Scalability benchmark
This benchmark starts N producers and N consumers (NxN below). The total throughput in msg/sec is measured.
Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
---- Running throughput and throughput benchmarks (higher is better) ----
        boost::lockfree::queue, 2x2:   2,486,910 msg/sec (mean:   2,320,783 stdev:     188,366) msg/sec
        boost::lockfree::queue, 3x3:   2,332,429 msg/sec (mean:   2,076,063 stdev:     205,316) msg/sec
        boost::lockfree::queue, 4x4:   2,416,385 msg/sec (mean:   1,926,786 stdev:     229,553) msg/sec
        boost::lockfree::queue, 5x5:   2,075,041 msg/sec (mean:   1,837,805 stdev:     151,124) msg/sec
        boost::lockfree::queue, 6x6:   2,012,571 msg/sec (mean:   1,682,966 stdev:     143,713) msg/sec
 tbb::concurrent_bounded_queue, 2x2:   6,208,394 msg/sec (mean:   4,637,585 stdev:     967,385) msg/sec
 tbb::concurrent_bounded_queue, 3x3:   6,305,786 msg/sec (mean:   4,713,897 stdev:     956,974) msg/sec
 tbb::concurrent_bounded_queue, 4x4:   5,876,249 msg/sec (mean:   5,005,050 stdev:     588,870) msg/sec
 tbb::concurrent_bounded_queue, 5x5:   6,738,452 msg/sec (mean:   5,634,938 stdev:     738,323) msg/sec
 tbb::concurrent_bounded_queue, 6x6:   6,840,621 msg/sec (mean:   6,019,159 stdev:     613,073) msg/sec
              pthread_spinlock, 2x2:   5,195,250 msg/sec (mean:   3,935,210 stdev:   1,042,905) msg/sec
              pthread_spinlock, 3x3:   3,478,066 msg/sec (mean:   1,830,504 stdev:     839,054) msg/sec
              pthread_spinlock, 4x4:   2,386,709 msg/sec (mean:   1,405,528 stdev:     632,886) msg/sec
              pthread_spinlock, 5x5:   1,887,785 msg/sec (mean:   1,021,789 stdev:     344,305) msg/sec
              pthread_spinlock, 6x6:   1,977,791 msg/sec (mean:   1,247,068 stdev:     433,732) msg/sec
                   AtomicQueue, 2x2:   5,088,592 msg/sec (mean:   4,483,912 stdev:     378,409) msg/sec
                   AtomicQueue, 3x3:   3,858,049 msg/sec (mean:   3,401,465 stdev:     275,586) msg/sec
                   AtomicQueue, 4x4:   3,194,399 msg/sec (mean:   2,679,596 stdev:     370,335) msg/sec
                   AtomicQueue, 5x5:   2,964,360 msg/sec (mean:   2,383,516 stdev:     358,904) msg/sec
                   AtomicQueue, 6x6:   2,805,196 msg/sec (mean:   2,175,421 stdev:     386,756) msg/sec
           BlockingAtomicQueue, 2x2:  15,216,691 msg/sec (mean:  11,255,366 stdev:   2,144,351) msg/sec
           BlockingAtomicQueue, 3x3:  16,189,937 msg/sec (mean:  12,743,020 stdev:   1,751,287) msg/sec
           BlockingAtomicQueue, 4x4:  20,820,568 msg/sec (mean:  16,415,916 stdev:   3,860,112) msg/sec
           BlockingAtomicQueue, 5x5:  21,542,593 msg/sec (mean:  17,352,998 stdev:   4,193,881) msg/sec
           BlockingAtomicQueue, 6x6:  21,491,321 msg/sec (mean:  17,558,517 stdev:   3,824,894) msg/sec
                  AtomicQueue2, 2x2:   4,881,916 msg/sec (mean:   4,446,936 stdev:     354,687) msg/sec
                  AtomicQueue2, 3x3:   3,760,442 msg/sec (mean:   3,343,958 stdev:     295,021) msg/sec
                  AtomicQueue2, 4x4:   3,152,725 msg/sec (mean:   2,661,291 stdev:     236,837) msg/sec
                  AtomicQueue2, 5x5:   3,192,256 msg/sec (mean:   2,466,403 stdev:     371,514) msg/sec
                  AtomicQueue2, 6x6:   2,551,024 msg/sec (mean:   2,134,442 stdev:     322,944) msg/sec
          BlockingAtomicQueue2, 2x2:  11,847,354 msg/sec (mean:   9,194,327 stdev:   1,883,534) msg/sec
          BlockingAtomicQueue2, 3x3:  13,050,469 msg/sec (mean:  11,457,476 stdev:   1,148,692) msg/sec
          BlockingAtomicQueue2, 4x4:  19,967,380 msg/sec (mean:  13,552,963 stdev:   3,569,927) msg/sec
          BlockingAtomicQueue2, 5x5:  19,042,976 msg/sec (mean:  12,586,241 stdev:   2,894,310) msg/sec
          BlockingAtomicQueue2, 6x6:  19,970,175 msg/sec (mean:  14,714,509 stdev:   3,414,408) msg/sec
