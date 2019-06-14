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

Results on Intel Core i7-7700K 5GHz, Ubuntu 18.04.2 LTS:
```
     boost::lockfree::spsc_queue: 0.000000094 sec/round-trip (mean: 0.000000120 stdev: 0.000000005)
          boost::lockfree::queue: 0.000000257 sec/round-trip (mean: 0.000000275 stdev: 0.000000009)
                pthread_spinlock: 0.000000215 sec/round-trip (mean: 0.000005276 stdev: 0.000020808)
     moodycamel::ConcurrentQueue: 0.000000200 sec/round-trip (mean: 0.000000211 stdev: 0.000000005)
                 tbb::spin_mutex: 0.000000178 sec/round-trip (mean: 0.000000212 stdev: 0.000000024)
     tbb::speculative_spin_mutex: 0.000000410 sec/round-trip (mean: 0.000000923 stdev: 0.000000623)
   tbb::concurrent_bounded_queue: 0.000000249 sec/round-trip (mean: 0.000000253 stdev: 0.000000002)
                     AtomicQueue: 0.000000146 sec/round-trip (mean: 0.000000156 stdev: 0.000000005)
             OptimistAtomicQueue: 0.000000088 sec/round-trip (mean: 0.000000104 stdev: 0.000000010)
                    AtomicQueue2: 0.000000181 sec/round-trip (mean: 0.000000193 stdev: 0.000000006)
            OptimistAtomicQueue2: 0.000000151 sec/round-trip (mean: 0.000000165 stdev: 0.000000010)
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
     boost::lockfree::spsc_queue: 0.000000280 sec/round-trip (mean: 0.000000353 stdev: 0.000000058)
          boost::lockfree::queue: 0.000000642 sec/round-trip (mean: 0.000000749 stdev: 0.000000074)
                pthread_spinlock: 0.000000300 sec/round-trip (mean: 0.000000520 stdev: 0.000000127)
     moodycamel::ConcurrentQueue: 0.000000436 sec/round-trip (mean: 0.000000482 stdev: 0.000000025)
                 tbb::spin_mutex: 0.000000257 sec/round-trip (mean: 0.000000436 stdev: 0.000000099)
     tbb::speculative_spin_mutex: 0.000000588 sec/round-trip (mean: 0.000000758 stdev: 0.000000137)
   tbb::concurrent_bounded_queue: 0.000000594 sec/round-trip (mean: 0.000000615 stdev: 0.000000016)
                     AtomicQueue: 0.000000345 sec/round-trip (mean: 0.000000389 stdev: 0.000000024)
             OptimistAtomicQueue: 0.000000204 sec/round-trip (mean: 0.000000224 stdev: 0.000000012)
                    AtomicQueue2: 0.000000400 sec/round-trip (mean: 0.000000463 stdev: 0.000000038)
            OptimistAtomicQueue2: 0.000000319 sec/round-trip (mean: 0.000000372 stdev: 0.000000035)
```

## Throughput and scalability benchmark
N producer threads push a 4-byte integer into one queue, N consumer threads pop the integers from the queue. Each producer posts 1,000,000 messages. Total time to send and receive all the messages is measured. The benchmark is run for from 1 producer and 1 consumer up to `(total-number-of-cpus / 2 - 1)` producers/consumers to measure the scalabilty of different queues.

Results on Intel Core i7-7700K 5GHz, Ubuntu 18.04.2 LTS (up to 3 producers and 3 consumers):
```
   boost::lockfree::spsc_queue,1: 251,997,545 msg/sec (mean:  66,736,525 stdev:  46,551,058)
        boost::lockfree::queue,1:   9,661,869 msg/sec (mean:   9,033,010 stdev:     301,844)
        boost::lockfree::queue,2:   8,213,342 msg/sec (mean:   7,965,616 stdev:      95,219)
        boost::lockfree::queue,3:   8,273,477 msg/sec (mean:   7,703,927 stdev:     295,292)
              pthread_spinlock,1:  41,738,740 msg/sec (mean:  21,418,909 stdev:  11,856,605)
              pthread_spinlock,2:  18,508,600 msg/sec (mean:  14,234,517 stdev:   1,776,114)
              pthread_spinlock,3:  14,830,999 msg/sec (mean:  12,242,527 stdev:   1,334,781)
   moodycamel::ConcurrentQueue,1:  20,245,021 msg/sec (mean:  17,905,627 stdev:   1,324,008)
   moodycamel::ConcurrentQueue,2:  12,820,195 msg/sec (mean:  11,452,141 stdev:     352,443)
   moodycamel::ConcurrentQueue,3:  15,885,204 msg/sec (mean:  14,415,569 stdev:     424,747)
               tbb::spin_mutex,1:  84,115,672 msg/sec (mean:  79,986,367 stdev:   2,030,866)
               tbb::spin_mutex,2:  56,798,609 msg/sec (mean:  53,635,495 stdev:   1,447,946)
               tbb::spin_mutex,3:  39,322,375 msg/sec (mean:  38,417,353 stdev:     468,802)
   tbb::speculative_spin_mutex,1:  55,890,715 msg/sec (mean:  44,089,801 stdev:  11,063,936)
   tbb::speculative_spin_mutex,2:  38,532,479 msg/sec (mean:  35,446,487 stdev:   2,132,127)
   tbb::speculative_spin_mutex,3:  31,403,577 msg/sec (mean:  30,133,949 stdev:     553,133)
 tbb::concurrent_bounded_queue,1:  15,636,497 msg/sec (mean:  15,103,239 stdev:     327,430)
 tbb::concurrent_bounded_queue,2:  15,905,864 msg/sec (mean:  15,093,753 stdev:     362,302)
 tbb::concurrent_bounded_queue,3:  14,566,885 msg/sec (mean:  12,841,410 stdev:     403,090)
                   AtomicQueue,1:  27,480,485 msg/sec (mean:  21,620,085 stdev:   3,089,085)
                   AtomicQueue,2:  14,569,157 msg/sec (mean:  13,275,562 stdev:     814,508)
                   AtomicQueue,3:  12,931,967 msg/sec (mean:  12,060,323 stdev:     548,793)
           OptimistAtomicQueue,1: 119,346,550 msg/sec (mean:  94,219,439 stdev:  22,267,824)
           OptimistAtomicQueue,2:  46,267,662 msg/sec (mean:  40,535,588 stdev:   3,454,206)
           OptimistAtomicQueue,3:  53,199,578 msg/sec (mean:  37,737,803 stdev:   2,957,516)
                  AtomicQueue2,1:  26,452,767 msg/sec (mean:  21,764,050 stdev:   2,604,954)
                  AtomicQueue2,2:  14,616,236 msg/sec (mean:  13,030,890 stdev:     933,939)
                  AtomicQueue2,3:  12,823,498 msg/sec (mean:  11,878,783 stdev:     542,021)
          OptimistAtomicQueue2,1:  75,140,383 msg/sec (mean:  46,821,449 stdev:  11,226,903)
          OptimistAtomicQueue2,2:  38,691,918 msg/sec (mean:  34,771,236 stdev:   2,762,252)
          OptimistAtomicQueue2,3:  38,757,784 msg/sec (mean:  36,395,621 stdev:     723,460)
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago):
```
   boost::lockfree::spsc_queue,1: 103,053,409 msg/sec (mean:  31,298,399 stdev:  18,864,401)
        boost::lockfree::queue,1:   3,266,871 msg/sec (mean:   2,552,799 stdev:     468,577)
        boost::lockfree::queue,2:   2,542,323 msg/sec (mean:   2,000,750 stdev:     247,595)
        boost::lockfree::queue,3:   2,345,333 msg/sec (mean:   1,832,748 stdev:     210,754)
        boost::lockfree::queue,4:   2,182,187 msg/sec (mean:   1,744,914 stdev:     188,613)
        boost::lockfree::queue,5:   2,003,591 msg/sec (mean:   1,639,274 stdev:     132,687)
        boost::lockfree::queue,6:   1,863,143 msg/sec (mean:   1,505,331 stdev:     183,726)
        boost::lockfree::queue,7:   1,382,560 msg/sec (mean:     981,282 stdev:     114,932)
        boost::lockfree::queue,8:     871,152 msg/sec (mean:     743,823 stdev:      72,281)
        boost::lockfree::queue,9:     877,899 msg/sec (mean:     705,637 stdev:      72,338)
       boost::lockfree::queue,10:     822,737 msg/sec (mean:     688,582 stdev:      65,785)
       boost::lockfree::queue,11:     832,595 msg/sec (mean:     643,272 stdev:      68,854)
       boost::lockfree::queue,12:     702,563 msg/sec (mean:     611,362 stdev:      43,111)
       boost::lockfree::queue,13:     708,480 msg/sec (mean:     614,818 stdev:      37,941)
              pthread_spinlock,1:   8,638,980 msg/sec (mean:   6,674,125 stdev:   1,084,293)
              pthread_spinlock,2:   4,979,835 msg/sec (mean:   3,123,822 stdev:     871,628)
              pthread_spinlock,3:   3,722,054 msg/sec (mean:   2,215,033 stdev:     630,059)
              pthread_spinlock,4:   2,645,192 msg/sec (mean:   1,494,368 stdev:     357,393)
              pthread_spinlock,5:   1,835,128 msg/sec (mean:   1,250,598 stdev:     280,842)
              pthread_spinlock,6:   2,022,431 msg/sec (mean:   1,110,794 stdev:     241,193)
              pthread_spinlock,7:   1,119,543 msg/sec (mean:     597,228 stdev:     143,430)
              pthread_spinlock,8:     890,076 msg/sec (mean:     501,356 stdev:      94,951)
              pthread_spinlock,9:     842,013 msg/sec (mean:     485,299 stdev:      84,871)
             pthread_spinlock,10:     585,498 msg/sec (mean:     489,405 stdev:      50,472)
             pthread_spinlock,11:     730,614 msg/sec (mean:     503,594 stdev:      66,617)
             pthread_spinlock,12:     628,214 msg/sec (mean:     526,111 stdev:      39,693)
             pthread_spinlock,13:   1,018,729 msg/sec (mean:     642,041 stdev:     114,443)
   moodycamel::ConcurrentQueue,1:   7,914,035 msg/sec (mean:   5,920,331 stdev:     825,359)
   moodycamel::ConcurrentQueue,2:   5,551,079 msg/sec (mean:   4,737,300 stdev:     521,087)
   moodycamel::ConcurrentQueue,3:   5,409,742 msg/sec (mean:   4,761,520 stdev:     393,264)
   moodycamel::ConcurrentQueue,4:   5,573,217 msg/sec (mean:   4,909,601 stdev:     351,440)
   moodycamel::ConcurrentQueue,5:   5,887,913 msg/sec (mean:   5,282,487 stdev:     280,373)
   moodycamel::ConcurrentQueue,6:   6,093,806 msg/sec (mean:   5,621,881 stdev:     319,946)
   moodycamel::ConcurrentQueue,7:   3,982,560 msg/sec (mean:   3,834,764 stdev:     129,542)
   moodycamel::ConcurrentQueue,8:   4,000,741 msg/sec (mean:   3,664,307 stdev:     216,342)
   moodycamel::ConcurrentQueue,9:   4,134,115 msg/sec (mean:   3,857,163 stdev:     188,971)
  moodycamel::ConcurrentQueue,10:   4,222,205 msg/sec (mean:   3,896,444 stdev:     241,321)
  moodycamel::ConcurrentQueue,11:   4,229,825 msg/sec (mean:   4,037,034 stdev:     166,835)
  moodycamel::ConcurrentQueue,12:   4,310,331 msg/sec (mean:   4,017,198 stdev:     214,482)
  moodycamel::ConcurrentQueue,13:   4,543,981 msg/sec (mean:   4,062,894 stdev:     267,152)
               tbb::spin_mutex,1:  22,641,671 msg/sec (mean:  17,786,673 stdev:   2,467,424)
               tbb::spin_mutex,2:  11,094,817 msg/sec (mean:   9,456,857 stdev:     711,443)
               tbb::spin_mutex,3:   6,865,243 msg/sec (mean:   6,383,285 stdev:     353,116)
               tbb::spin_mutex,4:   5,571,108 msg/sec (mean:   4,255,001 stdev:     529,932)
               tbb::spin_mutex,5:   4,299,743 msg/sec (mean:   3,487,510 stdev:     548,465)
               tbb::spin_mutex,6:   3,444,430 msg/sec (mean:   2,571,645 stdev:     315,035)
               tbb::spin_mutex,7:   1,739,030 msg/sec (mean:   1,365,773 stdev:     197,808)
               tbb::spin_mutex,8:     951,427 msg/sec (mean:     737,953 stdev:      86,657)
               tbb::spin_mutex,9:     654,755 msg/sec (mean:     541,638 stdev:      54,541)
              tbb::spin_mutex,10:     605,509 msg/sec (mean:     459,797 stdev:      65,264)
              tbb::spin_mutex,11:     574,943 msg/sec (mean:     419,224 stdev:      52,062)
              tbb::spin_mutex,12:     556,493 msg/sec (mean:     398,216 stdev:      58,632)
              tbb::spin_mutex,13:     464,592 msg/sec (mean:     356,835 stdev:      46,392)
   tbb::speculative_spin_mutex,1:  21,898,534 msg/sec (mean:  15,898,925 stdev:   3,728,173)
   tbb::speculative_spin_mutex,2:  12,132,171 msg/sec (mean:   7,767,802 stdev:   1,643,216)
   tbb::speculative_spin_mutex,3:   8,235,042 msg/sec (mean:   6,353,998 stdev:     713,505)
   tbb::speculative_spin_mutex,4:   6,002,924 msg/sec (mean:   4,573,655 stdev:     535,132)
   tbb::speculative_spin_mutex,5:   6,002,881 msg/sec (mean:   3,948,315 stdev:     524,602)
   tbb::speculative_spin_mutex,6:   4,837,971 msg/sec (mean:   3,364,177 stdev:     563,687)
   tbb::speculative_spin_mutex,7:   2,772,219 msg/sec (mean:   1,855,448 stdev:     504,930)
   tbb::speculative_spin_mutex,8:   1,827,043 msg/sec (mean:   1,186,316 stdev:     506,039)
   tbb::speculative_spin_mutex,9:   1,515,169 msg/sec (mean:   1,005,084 stdev:     420,159)
  tbb::speculative_spin_mutex,10:   1,446,513 msg/sec (mean:     965,714 stdev:     371,964)
  tbb::speculative_spin_mutex,11:   1,383,558 msg/sec (mean:     912,254 stdev:     339,773)
  tbb::speculative_spin_mutex,12:   1,345,158 msg/sec (mean:     906,724 stdev:     288,765)
  tbb::speculative_spin_mutex,13:   1,318,716 msg/sec (mean:     868,000 stdev:     251,893)
 tbb::concurrent_bounded_queue,1:   6,135,313 msg/sec (mean:   5,375,108 stdev:     716,125)
 tbb::concurrent_bounded_queue,2:   5,114,928 msg/sec (mean:   4,148,583 stdev:     379,484)
 tbb::concurrent_bounded_queue,3:   3,527,494 msg/sec (mean:   3,244,601 stdev:     180,174)
 tbb::concurrent_bounded_queue,4:   3,166,730 msg/sec (mean:   2,845,753 stdev:     151,315)
 tbb::concurrent_bounded_queue,5:   3,165,826 msg/sec (mean:   2,643,790 stdev:     181,739)
 tbb::concurrent_bounded_queue,6:   2,716,783 msg/sec (mean:   2,379,424 stdev:     148,604)
 tbb::concurrent_bounded_queue,7:   1,410,419 msg/sec (mean:   1,307,720 stdev:      45,732)
 tbb::concurrent_bounded_queue,8:   1,149,550 msg/sec (mean:   1,042,017 stdev:      49,525)
 tbb::concurrent_bounded_queue,9:   1,066,826 msg/sec (mean:     966,312 stdev:      47,731)
tbb::concurrent_bounded_queue,10:   1,039,772 msg/sec (mean:     923,785 stdev:      61,811)
tbb::concurrent_bounded_queue,11:   1,176,357 msg/sec (mean:     893,225 stdev:      66,265)
tbb::concurrent_bounded_queue,12:   1,132,745 msg/sec (mean:     895,794 stdev:      69,115)
tbb::concurrent_bounded_queue,13:   1,222,088 msg/sec (mean:     898,824 stdev:      71,407)
                   AtomicQueue,1:   8,651,668 msg/sec (mean:   6,589,054 stdev:     927,527)
                   AtomicQueue,2:   4,968,941 msg/sec (mean:   3,706,211 stdev:     580,877)
                   AtomicQueue,3:   3,683,433 msg/sec (mean:   2,851,401 stdev:     351,454)
                   AtomicQueue,4:   3,132,166 msg/sec (mean:   2,501,759 stdev:     318,839)
                   AtomicQueue,5:   2,904,792 msg/sec (mean:   2,284,777 stdev:     330,472)
                   AtomicQueue,6:   2,501,441 msg/sec (mean:   2,069,747 stdev:     312,847)
                   AtomicQueue,7:   1,440,061 msg/sec (mean:   1,139,902 stdev:     130,746)
                   AtomicQueue,8:   1,088,100 msg/sec (mean:     931,964 stdev:     124,624)
                   AtomicQueue,9:   1,063,403 msg/sec (mean:     878,978 stdev:     118,421)
                  AtomicQueue,10:   1,108,646 msg/sec (mean:     838,869 stdev:     119,948)
                  AtomicQueue,11:   1,036,841 msg/sec (mean:     816,904 stdev:     115,519)
                  AtomicQueue,12:   1,078,988 msg/sec (mean:     827,803 stdev:     123,915)
                  AtomicQueue,13:   1,180,694 msg/sec (mean:     847,368 stdev:     138,499)
           OptimistAtomicQueue,1:  59,693,531 msg/sec (mean:  22,762,812 stdev:  15,145,833)
           OptimistAtomicQueue,2:  11,800,036 msg/sec (mean:   8,972,437 stdev:   1,263,817)
           OptimistAtomicQueue,3:  14,568,786 msg/sec (mean:  10,476,116 stdev:   2,114,945)
           OptimistAtomicQueue,4:  15,321,197 msg/sec (mean:  11,341,814 stdev:   2,415,766)
           OptimistAtomicQueue,5:  15,140,085 msg/sec (mean:  11,998,579 stdev:   2,269,305)
           OptimistAtomicQueue,6:  19,606,221 msg/sec (mean:  13,316,182 stdev:   2,813,488)
           OptimistAtomicQueue,7:  14,671,926 msg/sec (mean:  10,202,378 stdev:   1,864,250)
           OptimistAtomicQueue,8:  15,203,509 msg/sec (mean:  11,391,972 stdev:   2,944,044)
           OptimistAtomicQueue,9:  15,671,514 msg/sec (mean:  11,415,476 stdev:   3,151,627)
          OptimistAtomicQueue,10:  16,153,284 msg/sec (mean:  11,641,816 stdev:   3,300,332)
          OptimistAtomicQueue,11:  16,560,016 msg/sec (mean:  11,824,417 stdev:   3,405,276)
          OptimistAtomicQueue,12:  17,896,080 msg/sec (mean:  12,112,173 stdev:   3,932,033)
          OptimistAtomicQueue,13:  21,504,122 msg/sec (mean:  13,923,061 stdev:   6,070,772)
                  AtomicQueue2,1:  10,130,602 msg/sec (mean:   7,318,949 stdev:   1,148,938)
                  AtomicQueue2,2:   5,003,208 msg/sec (mean:   3,664,508 stdev:     645,104)
                  AtomicQueue2,3:   3,548,190 msg/sec (mean:   2,761,750 stdev:     357,122)
                  AtomicQueue2,4:   2,855,854 msg/sec (mean:   2,412,513 stdev:     282,717)
                  AtomicQueue2,5:   2,769,759 msg/sec (mean:   2,245,553 stdev:     343,284)
                  AtomicQueue2,6:   2,673,378 msg/sec (mean:   2,045,859 stdev:     363,524)
                  AtomicQueue2,7:   1,336,336 msg/sec (mean:   1,147,236 stdev:     116,458)
                  AtomicQueue2,8:   1,127,980 msg/sec (mean:     932,153 stdev:     109,788)
                  AtomicQueue2,9:   1,070,180 msg/sec (mean:     879,438 stdev:     106,949)
                 AtomicQueue2,10:   1,072,871 msg/sec (mean:     842,716 stdev:     120,125)
                 AtomicQueue2,11:   1,115,495 msg/sec (mean:     818,013 stdev:     118,519)
                 AtomicQueue2,12:   1,140,078 msg/sec (mean:     830,333 stdev:     131,305)
                 AtomicQueue2,13:   1,249,730 msg/sec (mean:     864,061 stdev:     146,864)
          OptimistAtomicQueue2,1:  26,168,905 msg/sec (mean:   9,177,001 stdev:   3,598,507)
          OptimistAtomicQueue2,2:  12,074,730 msg/sec (mean:   7,173,045 stdev:   1,603,749)
          OptimistAtomicQueue2,3:  15,164,677 msg/sec (mean:   9,238,944 stdev:   1,847,296)
          OptimistAtomicQueue2,4:  16,067,618 msg/sec (mean:  10,342,609 stdev:   2,299,330)
          OptimistAtomicQueue2,5:  17,905,619 msg/sec (mean:  10,753,262 stdev:   2,572,491)
          OptimistAtomicQueue2,6:  14,028,380 msg/sec (mean:  11,009,890 stdev:   1,767,214)
          OptimistAtomicQueue2,7:  16,145,968 msg/sec (mean:  10,115,926 stdev:   2,093,751)
          OptimistAtomicQueue2,8:  14,583,260 msg/sec (mean:  10,943,199 stdev:   2,959,249)
          OptimistAtomicQueue2,9:  15,337,181 msg/sec (mean:  11,044,559 stdev:   3,285,722)
         OptimistAtomicQueue2,10:  15,643,690 msg/sec (mean:  11,377,700 stdev:   3,693,233)
         OptimistAtomicQueue2,11:  16,158,847 msg/sec (mean:  11,585,951 stdev:   3,961,141)
         OptimistAtomicQueue2,12:  17,669,598 msg/sec (mean:  12,139,260 stdev:   4,040,177)
         OptimistAtomicQueue2,13:  21,016,313 msg/sec (mean:  13,275,243 stdev:   5,207,114)
```

(C) Maxim Egorushkin 2019

[1]: https://max0x7ba.github.io/atomic_queue/html/benchmarks.html
