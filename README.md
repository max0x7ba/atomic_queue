# atomic_queue
Multiple producer multiple consumer C++14 *lock-free* queues based on `std::atomic<>`. The queues employ busy loops, so they are not *wait-free*.

The main idea these queues utilize is _simplicity_: fixed size buffer, busy wait.

These qualities are also limitations: the maximum queue size must be set at compile time, there are no blocking push/pop functionality. Nevertheless, ultra-low-latency applications need just that and nothing more. The simplicity pays off (see the [throughput and latency benchmarks][1]).

Available containers are:
* `AtomicQueue` - a fixed size ring-buffer for atomic elements.
* `BlockingAtomicQueue` - a faster fixed size ring-buffer for atomic elements which busy-waits when empty or full.
* `AtomicQueue2` - a fixed size ring-buffer for non-atomic elements.
* `BlockingAtomicQueue2` - a faster fixed size ring-buffer for non-atomic elements which busy-waits when empty or full.

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

Results on Intel Core i7-7700K 5GHz, Ubuntu 18.04.2 LTS:
```
     boost::lockfree::spsc_queue: 0.000000104 sec/round-trip (mean: 0.000000122 stdev: 0.000000006)
          boost::lockfree::queue: 0.000000269 sec/round-trip (mean: 0.000000289 stdev: 0.000000010)
                pthread_spinlock: 0.000000397 sec/round-trip (mean: 0.000001894 stdev: 0.000004674)
     moodycamel::ConcurrentQueue: 0.000000191 sec/round-trip (mean: 0.000000198 stdev: 0.000000004)
                 tbb::spin_mutex: 0.000000623 sec/round-trip (mean: 0.000001304 stdev: 0.000000298)
     tbb::speculative_spin_mutex: 0.000000502 sec/round-trip (mean: 0.000002322 stdev: 0.000002635)
   tbb::concurrent_bounded_queue: 0.000000248 sec/round-trip (mean: 0.000000255 stdev: 0.000000003)
                     AtomicQueue: 0.000000142 sec/round-trip (mean: 0.000000159 stdev: 0.000000007)
             BlockingAtomicQueue: 0.000000093 sec/round-trip (mean: 0.000000106 stdev: 0.000000008)
                    AtomicQueue2: 0.000000170 sec/round-trip (mean: 0.000000186 stdev: 0.000000007)
            BlockingAtomicQueue2: 0.000000184 sec/round-trip (mean: 0.000000198 stdev: 0.000000005)
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
     boost::lockfree::spsc_queue: 0.000000311 sec/round-trip (mean: 0.000000378 stdev: 0.000000064)
          boost::lockfree::queue: 0.000000661 sec/round-trip (mean: 0.000000741 stdev: 0.000000052)
                pthread_spinlock: 0.000001114 sec/round-trip (mean: 0.000001242 stdev: 0.000000094)
     moodycamel::ConcurrentQueue: 0.000000451 sec/round-trip (mean: 0.000000468 stdev: 0.000000016)
                 tbb::spin_mutex: 0.000001995 sec/round-trip (mean: 0.000002187 stdev: 0.000000149)
     tbb::speculative_spin_mutex: 0.000002353 sec/round-trip (mean: 0.000002931 stdev: 0.000000610)
   tbb::concurrent_bounded_queue: 0.000000611 sec/round-trip (mean: 0.000000644 stdev: 0.000000030)
                     AtomicQueue: 0.000000342 sec/round-trip (mean: 0.000000390 stdev: 0.000000028)
             BlockingAtomicQueue: 0.000000211 sec/round-trip (mean: 0.000000233 stdev: 0.000000018)
                    AtomicQueue2: 0.000000429 sec/round-trip (mean: 0.000000483 stdev: 0.000000032)
            BlockingAtomicQueue2: 0.000000377 sec/round-trip (mean: 0.000000405 stdev: 0.000000024)
```

## Throughput and scalability benchmark
N producer threads push a 4-byte integer into one queue, N consumer threads pop the integers from the queue. Each producer posts 1,000,000 messages. Total time to send and receive all the messages is measured. The benchmark is run for from 1 producer and 1 consumer up to `(total-number-of-cpus / 2 - 1)` producers/consumers to measure the scalabilty of different queues.

Results on Intel Core i7-7700K 5GHz, Ubuntu 18.04.2 LTS (up to 3 producers and 3 consumers):
```
   boost::lockfree::spsc_queue,1: 200,912,400 msg/sec (mean:  55,913,665 stdev:  29,080,515)
        boost::lockfree::queue,1:  11,023,483 msg/sec (mean:   8,891,171 stdev:     708,467)
        boost::lockfree::queue,2:   8,099,673 msg/sec (mean:   7,645,702 stdev:     149,323)
        boost::lockfree::queue,3:   8,206,546 msg/sec (mean:   7,536,162 stdev:     250,879)
              pthread_spinlock,1:  41,645,830 msg/sec (mean:  26,320,453 stdev:  13,257,379)
              pthread_spinlock,2:  15,673,894 msg/sec (mean:  12,444,315 stdev:   1,965,528)
              pthread_spinlock,3:  14,472,407 msg/sec (mean:  11,887,118 stdev:   1,060,958)
   moodycamel::ConcurrentQueue,1:  21,877,814 msg/sec (mean:  19,444,916 stdev:   1,348,017)
   moodycamel::ConcurrentQueue,2:  13,313,696 msg/sec (mean:  11,499,845 stdev:     437,919)
   moodycamel::ConcurrentQueue,3:  16,899,773 msg/sec (mean:  15,984,864 stdev:     797,766)
               tbb::spin_mutex,1:  84,869,062 msg/sec (mean:  76,475,045 stdev:   6,171,235)
               tbb::spin_mutex,2:  54,668,038 msg/sec (mean:  51,719,257 stdev:   2,075,092)
               tbb::spin_mutex,3:  37,958,525 msg/sec (mean:  36,439,365 stdev:   1,404,608)
   tbb::speculative_spin_mutex,1:  54,891,133 msg/sec (mean:  43,317,759 stdev:   8,101,107)
   tbb::speculative_spin_mutex,2:  38,183,081 msg/sec (mean:  34,324,437 stdev:   2,121,573)
   tbb::speculative_spin_mutex,3:  31,167,841 msg/sec (mean:  29,258,773 stdev:   1,274,343)
 tbb::concurrent_bounded_queue,1:  15,714,751 msg/sec (mean:  15,087,393 stdev:     417,337)
 tbb::concurrent_bounded_queue,2:  16,053,555 msg/sec (mean:  15,201,702 stdev:     364,394)
 tbb::concurrent_bounded_queue,3:  14,006,100 msg/sec (mean:  13,396,482 stdev:     493,767)
                   AtomicQueue,1:  27,042,489 msg/sec (mean:  21,581,720 stdev:   3,450,145)
                   AtomicQueue,2:  14,433,267 msg/sec (mean:  13,375,775 stdev:     865,119)
                   AtomicQueue,3:  13,174,470 msg/sec (mean:  12,001,608 stdev:     686,301)
           BlockingAtomicQueue,1: 118,101,900 msg/sec (mean: 101,335,735 stdev:  22,520,834)
           BlockingAtomicQueue,2:  48,069,873 msg/sec (mean:  41,438,055 stdev:   3,801,293)
           BlockingAtomicQueue,3:  45,423,479 msg/sec (mean:  38,890,828 stdev:   4,074,442)
                  AtomicQueue2,1:  26,956,305 msg/sec (mean:  22,534,377 stdev:   3,201,878)
                  AtomicQueue2,2:  14,416,246 msg/sec (mean:  13,183,150 stdev:     850,885)
                  AtomicQueue2,3:  13,116,088 msg/sec (mean:  11,947,091 stdev:     660,428)
          BlockingAtomicQueue2,1:  99,226,630 msg/sec (mean:  59,988,061 stdev:  25,054,014)
          BlockingAtomicQueue2,2:  39,737,559 msg/sec (mean:  35,894,714 stdev:   3,228,720)
          BlockingAtomicQueue2,3:  54,349,231 msg/sec (mean:  37,354,419 stdev:   3,331,709)
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago):
```
   boost::lockfree::spsc_queue,1:  28,647,057 msg/sec (mean:  22,693,074 stdev:   3,598,465)
        boost::lockfree::queue,1:   3,076,177 msg/sec (mean:   2,765,004 stdev:     195,521)
        boost::lockfree::queue,2:   2,585,288 msg/sec (mean:   2,338,816 stdev:     107,362)
        boost::lockfree::queue,3:   2,308,909 msg/sec (mean:   2,057,308 stdev:     153,187)
        boost::lockfree::queue,4:   2,077,551 msg/sec (mean:   1,967,348 stdev:     112,596)
        boost::lockfree::queue,5:   2,132,701 msg/sec (mean:   1,891,829 stdev:     137,626)
        boost::lockfree::queue,6:   1,910,673 msg/sec (mean:   1,749,657 stdev:      91,024)
        boost::lockfree::queue,7:   1,272,620 msg/sec (mean:   1,125,578 stdev:     125,300)
        boost::lockfree::queue,8:     995,984 msg/sec (mean:     889,595 stdev:      83,644)
        boost::lockfree::queue,9:     971,658 msg/sec (mean:     845,532 stdev:      77,976)
       boost::lockfree::queue,10:     935,901 msg/sec (mean:     811,837 stdev:      74,902)
       boost::lockfree::queue,11:     913,673 msg/sec (mean:     788,239 stdev:      77,656)
       boost::lockfree::queue,12:     981,122 msg/sec (mean:     799,710 stdev:     114,963)
       boost::lockfree::queue,13:     952,212 msg/sec (mean:     782,055 stdev:     101,111)
              pthread_spinlock,1:  11,181,241 msg/sec (mean:   8,678,774 stdev:   1,431,924)
              pthread_spinlock,2:   4,712,279 msg/sec (mean:   3,961,406 stdev:     737,401)
              pthread_spinlock,3:   2,948,019 msg/sec (mean:   2,427,866 stdev:     461,181)
              pthread_spinlock,4:   2,700,026 msg/sec (mean:   2,310,386 stdev:     289,120)
              pthread_spinlock,5:   2,152,777 msg/sec (mean:   1,601,107 stdev:     273,779)
              pthread_spinlock,6:   2,024,179 msg/sec (mean:   1,634,283 stdev:     190,858)
              pthread_spinlock,7:     928,253 msg/sec (mean:     763,768 stdev:      92,018)
              pthread_spinlock,8:   1,171,516 msg/sec (mean:     874,895 stdev:     177,677)
              pthread_spinlock,9:   1,004,067 msg/sec (mean:     804,082 stdev:     149,973)
             pthread_spinlock,10:     933,977 msg/sec (mean:     784,822 stdev:     119,769)
             pthread_spinlock,11:   1,092,945 msg/sec (mean:     878,627 stdev:     120,965)
             pthread_spinlock,12:     986,337 msg/sec (mean:     923,589 stdev:      46,840)
             pthread_spinlock,13:   1,014,406 msg/sec (mean:     934,062 stdev:      52,259)
   moodycamel::ConcurrentQueue,1:   9,114,627 msg/sec (mean:   8,308,408 stdev:     672,635)
   moodycamel::ConcurrentQueue,2:   7,268,810 msg/sec (mean:   6,665,790 stdev:     265,512)
   moodycamel::ConcurrentQueue,3:   6,919,174 msg/sec (mean:   6,269,986 stdev:     383,235)
   moodycamel::ConcurrentQueue,4:   6,846,519 msg/sec (mean:   6,421,347 stdev:     231,997)
   moodycamel::ConcurrentQueue,5:   7,140,036 msg/sec (mean:   6,614,685 stdev:     306,086)
   moodycamel::ConcurrentQueue,6:   6,757,518 msg/sec (mean:   6,494,175 stdev:     227,283)
   moodycamel::ConcurrentQueue,7:   5,071,657 msg/sec (mean:   4,339,176 stdev:     333,287)
   moodycamel::ConcurrentQueue,8:   4,363,453 msg/sec (mean:   4,196,728 stdev:     114,195)
   moodycamel::ConcurrentQueue,9:   4,343,497 msg/sec (mean:   4,207,567 stdev:     109,656)
  moodycamel::ConcurrentQueue,10:   4,453,191 msg/sec (mean:   4,284,342 stdev:     111,786)
  moodycamel::ConcurrentQueue,11:   4,527,464 msg/sec (mean:   4,273,049 stdev:     157,656)
  moodycamel::ConcurrentQueue,12:   4,537,660 msg/sec (mean:   4,242,330 stdev:     175,883)
  moodycamel::ConcurrentQueue,13:   4,666,488 msg/sec (mean:   4,440,837 stdev:     110,196)
               tbb::spin_mutex,1:  29,135,222 msg/sec (mean:  17,934,214 stdev:   4,739,509)
               tbb::spin_mutex,2:  15,306,010 msg/sec (mean:  10,665,850 stdev:   3,010,463)
               tbb::spin_mutex,3:  10,198,837 msg/sec (mean:   8,083,138 stdev:   1,425,041)
               tbb::spin_mutex,4:   5,864,980 msg/sec (mean:   4,742,709 stdev:     845,063)
               tbb::spin_mutex,5:   4,116,864 msg/sec (mean:   3,210,940 stdev:     613,440)
               tbb::spin_mutex,6:   2,988,234 msg/sec (mean:   2,266,884 stdev:     427,068)
               tbb::spin_mutex,7:   1,730,819 msg/sec (mean:   1,068,094 stdev:     292,511)
               tbb::spin_mutex,8:     972,850 msg/sec (mean:     793,698 stdev:     119,169)
               tbb::spin_mutex,9:     629,137 msg/sec (mean:     581,143 stdev:      40,334)
              tbb::spin_mutex,10:     540,488 msg/sec (mean:     463,466 stdev:      54,790)
              tbb::spin_mutex,11:     468,472 msg/sec (mean:     410,419 stdev:      44,053)
              tbb::spin_mutex,12:     421,827 msg/sec (mean:     353,267 stdev:      40,408)
              tbb::spin_mutex,13:     366,985 msg/sec (mean:     304,259 stdev:      39,443)
   tbb::speculative_spin_mutex,1:  25,288,998 msg/sec (mean:  20,585,201 stdev:   2,256,491)
   tbb::speculative_spin_mutex,2:  13,186,085 msg/sec (mean:   9,799,421 stdev:   2,499,323)
   tbb::speculative_spin_mutex,3:   8,743,569 msg/sec (mean:   7,171,561 stdev:   1,002,477)
   tbb::speculative_spin_mutex,4:   7,999,963 msg/sec (mean:   5,117,732 stdev:   1,236,482)
   tbb::speculative_spin_mutex,5:   3,985,715 msg/sec (mean:   3,565,111 stdev:     429,700)
   tbb::speculative_spin_mutex,6:   4,093,998 msg/sec (mean:   3,542,068 stdev:     313,860)
   tbb::speculative_spin_mutex,7:   2,441,846 msg/sec (mean:   1,872,755 stdev:     374,232)
   tbb::speculative_spin_mutex,8:   1,637,582 msg/sec (mean:   1,227,244 stdev:     269,996)
   tbb::speculative_spin_mutex,9:   1,410,427 msg/sec (mean:   1,091,426 stdev:     206,588)
  tbb::speculative_spin_mutex,10:   1,347,447 msg/sec (mean:   1,081,240 stdev:     198,834)
  tbb::speculative_spin_mutex,11:   1,204,232 msg/sec (mean:   1,041,802 stdev:     162,687)
  tbb::speculative_spin_mutex,12:   1,156,347 msg/sec (mean:     976,303 stdev:     166,562)
  tbb::speculative_spin_mutex,13:   1,172,530 msg/sec (mean:     944,933 stdev:     176,456)
 tbb::concurrent_bounded_queue,1:   6,765,902 msg/sec (mean:   6,142,626 stdev:     811,488)
 tbb::concurrent_bounded_queue,2:   5,241,217 msg/sec (mean:   4,742,619 stdev:     422,609)
 tbb::concurrent_bounded_queue,3:   4,004,545 msg/sec (mean:   3,641,471 stdev:     285,388)
 tbb::concurrent_bounded_queue,4:   3,515,826 msg/sec (mean:   3,184,182 stdev:     197,310)
 tbb::concurrent_bounded_queue,5:   3,003,594 msg/sec (mean:   2,881,030 stdev:     102,012)
 tbb::concurrent_bounded_queue,6:   2,819,324 msg/sec (mean:   2,659,355 stdev:      88,103)
 tbb::concurrent_bounded_queue,7:   1,932,210 msg/sec (mean:   1,484,911 stdev:     181,223)
 tbb::concurrent_bounded_queue,8:   1,232,510 msg/sec (mean:   1,118,334 stdev:      51,649)
 tbb::concurrent_bounded_queue,9:   1,195,961 msg/sec (mean:   1,082,382 stdev:      54,020)
tbb::concurrent_bounded_queue,10:   1,249,509 msg/sec (mean:   1,096,589 stdev:      73,672)
tbb::concurrent_bounded_queue,11:   1,310,556 msg/sec (mean:   1,144,796 stdev:      75,096)
tbb::concurrent_bounded_queue,12:   1,326,741 msg/sec (mean:   1,197,917 stdev:      74,431)
tbb::concurrent_bounded_queue,13:   1,346,596 msg/sec (mean:   1,238,757 stdev:      52,699)
                   AtomicQueue,1:  10,876,679 msg/sec (mean:   8,747,218 stdev:   1,102,805)
                   AtomicQueue,2:   5,180,138 msg/sec (mean:   4,442,608 stdev:     698,069)
                   AtomicQueue,3:   3,819,595 msg/sec (mean:   3,361,412 stdev:     384,166)
                   AtomicQueue,4:   3,217,122 msg/sec (mean:   2,758,063 stdev:     362,728)
                   AtomicQueue,5:   3,017,459 msg/sec (mean:   2,423,402 stdev:     371,827)
                   AtomicQueue,6:   2,722,561 msg/sec (mean:   2,226,434 stdev:     385,393)
                   AtomicQueue,7:   1,490,056 msg/sec (mean:   1,230,381 stdev:     126,115)
                   AtomicQueue,8:   1,278,662 msg/sec (mean:   1,066,826 stdev:     126,556)
                   AtomicQueue,9:   1,270,394 msg/sec (mean:   1,029,380 stdev:     148,501)
                  AtomicQueue,10:   1,255,603 msg/sec (mean:   1,005,427 stdev:     159,563)
                  AtomicQueue,11:   1,355,484 msg/sec (mean:   1,033,717 stdev:     200,534)
                  AtomicQueue,12:   1,341,954 msg/sec (mean:   1,046,170 stdev:     198,635)
                  AtomicQueue,13:   1,350,106 msg/sec (mean:   1,018,254 stdev:     211,751)
           BlockingAtomicQueue,1:  86,410,344 msg/sec (mean:  31,230,992 stdev:  24,726,622)
           BlockingAtomicQueue,2:  17,162,337 msg/sec (mean:  13,205,489 stdev:   2,754,161)
           BlockingAtomicQueue,3:  19,023,817 msg/sec (mean:  14,735,034 stdev:   2,961,968)
           BlockingAtomicQueue,4:  20,938,222 msg/sec (mean:  15,435,882 stdev:   3,890,728)
           BlockingAtomicQueue,5:  21,692,927 msg/sec (mean:  15,368,820 stdev:   3,950,987)
           BlockingAtomicQueue,6:  20,439,739 msg/sec (mean:  15,595,728 stdev:   3,515,589)
           BlockingAtomicQueue,7:  17,031,623 msg/sec (mean:  12,427,977 stdev:   2,896,894)
           BlockingAtomicQueue,8:  16,755,863 msg/sec (mean:  12,303,993 stdev:   3,573,294)
           BlockingAtomicQueue,9:  16,788,438 msg/sec (mean:  12,643,899 stdev:   3,417,971)
          BlockingAtomicQueue,10:  16,413,155 msg/sec (mean:  12,623,848 stdev:   3,409,084)
          BlockingAtomicQueue,11:  16,912,252 msg/sec (mean:  12,577,364 stdev:   3,384,598)
          BlockingAtomicQueue,12:  18,181,788 msg/sec (mean:  12,440,329 stdev:   4,239,337)
          BlockingAtomicQueue,13:  19,839,626 msg/sec (mean:  13,187,908 stdev:   5,468,174)
                  AtomicQueue2,1:  10,759,484 msg/sec (mean:   8,019,587 stdev:   1,508,048)
                  AtomicQueue2,2:   5,277,380 msg/sec (mean:   4,100,059 stdev:     691,213)
                  AtomicQueue2,3:   3,557,626 msg/sec (mean:   3,231,486 stdev:     236,954)
                  AtomicQueue2,4:   3,008,402 msg/sec (mean:   2,629,686 stdev:     294,975)
                  AtomicQueue2,5:   2,733,786 msg/sec (mean:   2,305,379 stdev:     317,369)
                  AtomicQueue2,6:   2,603,402 msg/sec (mean:   2,164,224 stdev:     321,805)
                  AtomicQueue2,7:   1,420,849 msg/sec (mean:   1,210,288 stdev:     110,186)
                  AtomicQueue2,8:   1,280,821 msg/sec (mean:   1,076,774 stdev:     107,586)
                  AtomicQueue2,9:   1,274,808 msg/sec (mean:   1,046,436 stdev:     130,689)
                 AtomicQueue2,10:   1,312,496 msg/sec (mean:   1,028,963 stdev:     159,621)
                 AtomicQueue2,11:   1,392,093 msg/sec (mean:   1,040,809 stdev:     194,492)
                 AtomicQueue2,12:   1,336,302 msg/sec (mean:   1,050,987 stdev:     184,558)
                 AtomicQueue2,13:   1,319,230 msg/sec (mean:   1,077,793 stdev:     159,467)
          BlockingAtomicQueue2,1:  26,950,145 msg/sec (mean:  16,322,670 stdev:   7,418,470)
          BlockingAtomicQueue2,2:  12,190,558 msg/sec (mean:  10,675,183 stdev:     992,340)
          BlockingAtomicQueue2,3:  13,485,867 msg/sec (mean:  11,205,558 stdev:   1,809,125)
          BlockingAtomicQueue2,4:  17,495,970 msg/sec (mean:  12,224,369 stdev:   2,886,940)
          BlockingAtomicQueue2,5:  16,804,263 msg/sec (mean:  13,789,346 stdev:   2,379,408)
          BlockingAtomicQueue2,6:  14,726,468 msg/sec (mean:  12,157,770 stdev:   1,508,241)
          BlockingAtomicQueue2,7:  14,647,858 msg/sec (mean:  11,334,545 stdev:   2,156,781)
          BlockingAtomicQueue2,8:  15,170,010 msg/sec (mean:  11,348,993 stdev:   3,254,143)
          BlockingAtomicQueue2,9:  15,439,516 msg/sec (mean:  11,484,726 stdev:   3,346,619)
         BlockingAtomicQueue2,10:  15,293,454 msg/sec (mean:  11,639,758 stdev:   3,255,057)
         BlockingAtomicQueue2,11:  16,381,086 msg/sec (mean:  11,802,579 stdev:   3,388,164)
         BlockingAtomicQueue2,12:  17,706,679 msg/sec (mean:  11,891,352 stdev:   4,118,196)
         BlockingAtomicQueue2,13:  16,665,156 msg/sec (mean:  11,348,928 stdev:   3,999,387)
```

(C) Maxim Egorushkin 2019

[1]: https://max0x7ba.github.io/atomic_queue/html/benchmarks.html
