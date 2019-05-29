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

## Ping-pong benchmark
One thread posts an integer to another thread and waits for the reply using two queues. The benchmarks measures the total time of 100,000 ping-pongs, best of 10 runs. Contention is minimal here to be able to achieve and measure the lowest latency. Reports the average round-trip time.

Results on Intel Core i7-7700K 5GHz, Ubuntu 18.04.2 LTS:
```
                pthread_spinlock: 0.000000338 sec/round-trip (mean: 0.000006725 stdev: 0.000016684)
     boost::lockfree::spsc_queue: 0.000000113 sec/round-trip (mean: 0.000000121 stdev: 0.000000005)
          boost::lockfree::queue: 0.000000262 sec/round-trip (mean: 0.000000281 stdev: 0.000000014)
                 tbb::spin_mutex: 0.000000940 sec/round-trip (mean: 0.000001318 stdev: 0.000000228)
     tbb::speculative_spin_mutex: 0.000000815 sec/round-trip (mean: 0.000003316 stdev: 0.000003579)
   tbb::concurrent_bounded_queue: 0.000000250 sec/round-trip (mean: 0.000000254 stdev: 0.000000002)
                     AtomicQueue: 0.000000146 sec/round-trip (mean: 0.000000160 stdev: 0.000000009)
             BlockingAtomicQueue: 0.000000094 sec/round-trip (mean: 0.000000104 stdev: 0.000000009)
                    AtomicQueue2: 0.000000177 sec/round-trip (mean: 0.000000185 stdev: 0.000000008)
            BlockingAtomicQueue2: 0.000000188 sec/round-trip (mean: 0.000000199 stdev: 0.000000006)
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago) (on one NUMA node):
```
                pthread_spinlock: 0.000001126 sec/round-trip (mean: 0.000001371 stdev: 0.000000110)
     boost::lockfree::spsc_queue: 0.000000267 sec/round-trip (mean: 0.000000330 stdev: 0.000000040)
          boost::lockfree::queue: 0.000000704 sec/round-trip (mean: 0.000000731 stdev: 0.000000029)
                 tbb::spin_mutex: 0.000001954 sec/round-trip (mean: 0.000002253 stdev: 0.000000178)
     tbb::speculative_spin_mutex: 0.000001894 sec/round-trip (mean: 0.000002871 stdev: 0.000000605)
   tbb::concurrent_bounded_queue: 0.000000622 sec/round-trip (mean: 0.000000652 stdev: 0.000000026)
                     AtomicQueue: 0.000000379 sec/round-trip (mean: 0.000000407 stdev: 0.000000020)
             BlockingAtomicQueue: 0.000000205 sec/round-trip (mean: 0.000000227 stdev: 0.000000015)
                    AtomicQueue2: 0.000000430 sec/round-trip (mean: 0.000000488 stdev: 0.000000035)
            BlockingAtomicQueue2: 0.000000349 sec/round-trip (mean: 0.000000426 stdev: 0.000000048)
```

## Throughput and scalability benchmark
N producer threads post into one queue, N consumer threads drain the queue. Each producer posts 1,000,000 messages. Total time to send and receive all the messages is measured. The benchmark is run for from 1 producer and 1 consumer up to `total-number-of-cpus / 2 - 1` producers/consumers to measure the scalabilty of different queues.

![Scalability Intel i7-7700k 5GHz](https://raw.githubusercontent.com/max0x7ba/atomic_queue/master/results/scalability-7700k-5GHz.png "Scalability Intel i7-7700k 5GHz") ![Intel Xeon Gold 6132](https://raw.githubusercontent.com/max0x7ba/atomic_queue/master/results/scalability-xeon-gold-6132.png "Intel Xeon Gold 6132")

Results on Intel Core i7-7700K 5GHz, Ubuntu 18.04.2 LTS (up to 3 producers and 3 consumers):
```
              pthread_spinlock,1:  41,729,933 msg/sec (mean:  26,683,936 stdev:  11,654,232)
              pthread_spinlock,2:  15,577,961 msg/sec (mean:  10,918,994 stdev:   4,056,484)
              pthread_spinlock,3:  14,945,822 msg/sec (mean:  12,344,251 stdev:   1,516,615)
        boost::lockfree::queue,1:  10,205,501 msg/sec (mean:   8,777,975 stdev:     519,013)
        boost::lockfree::queue,2:   7,970,549 msg/sec (mean:   7,571,607 stdev:     204,034)
        boost::lockfree::queue,3:   7,920,727 msg/sec (mean:   7,500,886 stdev:     223,848)
               tbb::spin_mutex,1:  85,232,248 msg/sec (mean:  75,835,769 stdev:   7,520,634)
               tbb::spin_mutex,2:  54,468,218 msg/sec (mean:  52,852,844 stdev:   1,160,117)
               tbb::spin_mutex,3:  38,036,453 msg/sec (mean:  36,639,812 stdev:   1,349,359)
   tbb::speculative_spin_mutex,1:  53,445,745 msg/sec (mean:  43,561,898 stdev:  10,963,816)
   tbb::speculative_spin_mutex,2:  39,245,530 msg/sec (mean:  36,096,034 stdev:   2,343,191)
   tbb::speculative_spin_mutex,3:  31,189,300 msg/sec (mean:  29,897,663 stdev:   1,349,058)
 tbb::concurrent_bounded_queue,1:  15,349,404 msg/sec (mean:  15,101,452 stdev:     168,659)
 tbb::concurrent_bounded_queue,2:  15,966,472 msg/sec (mean:  15,274,391 stdev:     338,877)
 tbb::concurrent_bounded_queue,3:  14,038,820 msg/sec (mean:  13,576,880 stdev:     319,065)
                   AtomicQueue,1:  27,473,115 msg/sec (mean:  20,220,784 stdev:   2,882,339)
                   AtomicQueue,2:  13,780,199 msg/sec (mean:  12,759,643 stdev:     537,253)
                   AtomicQueue,3:  13,532,478 msg/sec (mean:  11,716,300 stdev:     801,247)
           BlockingAtomicQueue,1: 117,895,405 msg/sec (mean: 116,294,485 stdev:   1,552,575)
           BlockingAtomicQueue,2:  43,575,992 msg/sec (mean:  38,487,993 stdev:   2,474,282)
           BlockingAtomicQueue,3:  36,987,120 msg/sec (mean:  35,985,071 stdev:     405,577)
                  AtomicQueue2,1:  25,092,312 msg/sec (mean:  21,651,330 stdev:   1,766,771)
                  AtomicQueue2,2:  14,274,779 msg/sec (mean:  12,587,898 stdev:     828,637)
                  AtomicQueue2,3:  12,566,061 msg/sec (mean:  11,592,397 stdev:     512,848)
          BlockingAtomicQueue2,1:  98,733,131 msg/sec (mean:  51,740,580 stdev:  22,082,502)
          BlockingAtomicQueue2,2:  38,478,447 msg/sec (mean:  35,316,568 stdev:   2,384,323)
          BlockingAtomicQueue2,3:  37,995,297 msg/sec (mean:  36,199,004 stdev:     892,665)
```

Results on Intel Xeon Gold 6132, Red Hat Enterprise Linux Server release 6.10 (Santiago):
```
              pthread_spinlock,1:  10,528,950 msg/sec (mean:   7,135,541 stdev:   1,656,530) msg/sec
              pthread_spinlock,2:   4,893,893 msg/sec (mean:   3,475,089 stdev:   1,301,925) msg/sec
              pthread_spinlock,3:   3,415,445 msg/sec (mean:   2,169,690 stdev:   1,020,208) msg/sec
              pthread_spinlock,4:   1,815,364 msg/sec (mean:   1,102,412 stdev:     299,150) msg/sec
              pthread_spinlock,5:   1,838,567 msg/sec (mean:   1,252,792 stdev:     380,737) msg/sec
              pthread_spinlock,6:   1,976,192 msg/sec (mean:   1,131,395 stdev:     374,738) msg/sec
              pthread_spinlock,7:   1,356,570 msg/sec (mean:     927,125 stdev:     182,245) msg/sec
              pthread_spinlock,8:     750,588 msg/sec (mean:     337,961 stdev:     168,515) msg/sec
              pthread_spinlock,9:     406,777 msg/sec (mean:     284,747 stdev:      74,412) msg/sec
             pthread_spinlock,10:     488,545 msg/sec (mean:     346,543 stdev:      70,152) msg/sec
             pthread_spinlock,11:     938,401 msg/sec (mean:     421,141 stdev:     191,404) msg/sec
             pthread_spinlock,12:     771,721 msg/sec (mean:     548,686 stdev:     101,946) msg/sec
             pthread_spinlock,13:     880,304 msg/sec (mean:     728,863 stdev:     108,598) msg/sec
        boost::lockfree::queue,1:   2,832,410 msg/sec (mean:   2,719,808 stdev:      71,558) msg/sec
        boost::lockfree::queue,2:   2,447,314 msg/sec (mean:   2,205,913 stdev:     185,500) msg/sec
        boost::lockfree::queue,3:   2,313,702 msg/sec (mean:   2,055,019 stdev:     165,980) msg/sec
        boost::lockfree::queue,4:   2,246,386 msg/sec (mean:   1,791,941 stdev:     170,484) msg/sec
        boost::lockfree::queue,5:   1,869,494 msg/sec (mean:   1,672,529 stdev:     128,212) msg/sec
        boost::lockfree::queue,6:   1,960,557 msg/sec (mean:   1,577,905 stdev:     169,535) msg/sec
        boost::lockfree::queue,7:   1,160,872 msg/sec (mean:     978,152 stdev:     124,778) msg/sec
        boost::lockfree::queue,8:     875,150 msg/sec (mean:     745,016 stdev:      60,285) msg/sec
        boost::lockfree::queue,9:     857,813 msg/sec (mean:     710,603 stdev:      66,984) msg/sec
       boost::lockfree::queue,10:     947,622 msg/sec (mean:     712,768 stdev:      93,074) msg/sec
       boost::lockfree::queue,11:     851,373 msg/sec (mean:     709,074 stdev:      61,122) msg/sec
       boost::lockfree::queue,12:     710,111 msg/sec (mean:     651,526 stdev:      37,881) msg/sec
       boost::lockfree::queue,13:     719,645 msg/sec (mean:     678,960 stdev:      23,743) msg/sec
               tbb::spin_mutex,1:  34,681,551 msg/sec (mean:  27,082,041 stdev:   6,139,291) msg/sec
               tbb::spin_mutex,2:  17,158,264 msg/sec (mean:  11,650,750 stdev:   3,082,065) msg/sec
               tbb::spin_mutex,3:  10,342,594 msg/sec (mean:   7,011,495 stdev:   1,591,270) msg/sec
               tbb::spin_mutex,4:   7,667,926 msg/sec (mean:   4,606,593 stdev:   1,306,123) msg/sec
               tbb::spin_mutex,5:   3,152,852 msg/sec (mean:   2,459,999 stdev:     398,700) msg/sec
               tbb::spin_mutex,6:   1,965,263 msg/sec (mean:   1,619,113 stdev:     305,671) msg/sec
               tbb::spin_mutex,7:     983,530 msg/sec (mean:     727,541 stdev:     136,859) msg/sec
               tbb::spin_mutex,8:     676,976 msg/sec (mean:     572,145 stdev:      56,698) msg/sec
               tbb::spin_mutex,9:     448,862 msg/sec (mean:     381,525 stdev:      42,106) msg/sec
              tbb::spin_mutex,10:     406,257 msg/sec (mean:     297,962 stdev:      49,150) msg/sec
              tbb::spin_mutex,11:     370,267 msg/sec (mean:     295,962 stdev:      48,509) msg/sec
              tbb::spin_mutex,12:     361,317 msg/sec (mean:     269,652 stdev:      48,871) msg/sec
              tbb::spin_mutex,13:     296,267 msg/sec (mean:     257,280 stdev:      32,800) msg/sec
   tbb::speculative_spin_mutex,1:  30,907,825 msg/sec (mean:  23,624,642 stdev:   3,401,649) msg/sec
   tbb::speculative_spin_mutex,2:  13,545,496 msg/sec (mean:  11,135,443 stdev:   1,196,123) msg/sec
   tbb::speculative_spin_mutex,3:   9,674,606 msg/sec (mean:   6,597,609 stdev:   1,247,501) msg/sec
   tbb::speculative_spin_mutex,4:   6,905,555 msg/sec (mean:   4,955,328 stdev:     927,885) msg/sec
   tbb::speculative_spin_mutex,5:   7,253,715 msg/sec (mean:   4,054,914 stdev:   1,431,125) msg/sec
   tbb::speculative_spin_mutex,6:   4,233,654 msg/sec (mean:   3,177,924 stdev:     513,674) msg/sec
   tbb::speculative_spin_mutex,7:   2,218,420 msg/sec (mean:   1,398,375 stdev:     550,866) msg/sec
   tbb::speculative_spin_mutex,8:   2,033,093 msg/sec (mean:     879,026 stdev:     516,417) msg/sec
   tbb::speculative_spin_mutex,9:   1,420,848 msg/sec (mean:     826,594 stdev:     307,698) msg/sec
  tbb::speculative_spin_mutex,10:   1,388,596 msg/sec (mean:     857,187 stdev:     322,385) msg/sec
  tbb::speculative_spin_mutex,11:   1,281,117 msg/sec (mean:     812,983 stdev:     247,808) msg/sec
  tbb::speculative_spin_mutex,12:   1,205,642 msg/sec (mean:     779,953 stdev:     235,769) msg/sec
  tbb::speculative_spin_mutex,13:   1,094,144 msg/sec (mean:     775,990 stdev:     194,991) msg/sec
 tbb::concurrent_bounded_queue,1:   6,098,542 msg/sec (mean:   5,814,600 stdev:     147,108) msg/sec
 tbb::concurrent_bounded_queue,2:   5,424,954 msg/sec (mean:   4,853,541 stdev:     464,175) msg/sec
 tbb::concurrent_bounded_queue,3:   3,894,589 msg/sec (mean:   3,448,634 stdev:     184,678) msg/sec
 tbb::concurrent_bounded_queue,4:   3,475,549 msg/sec (mean:   3,092,397 stdev:     219,271) msg/sec
 tbb::concurrent_bounded_queue,5:   3,135,563 msg/sec (mean:   2,829,418 stdev:     153,813) msg/sec
 tbb::concurrent_bounded_queue,6:   2,789,218 msg/sec (mean:   2,581,072 stdev:     145,911) msg/sec
 tbb::concurrent_bounded_queue,7:   1,484,382 msg/sec (mean:   1,335,447 stdev:      68,810) msg/sec
 tbb::concurrent_bounded_queue,8:   1,068,659 msg/sec (mean:   1,012,191 stdev:      44,403) msg/sec
 tbb::concurrent_bounded_queue,9:   1,029,527 msg/sec (mean:     948,962 stdev:      41,990) msg/sec
tbb::concurrent_bounded_queue,10:   1,041,493 msg/sec (mean:     936,649 stdev:      60,383) msg/sec
tbb::concurrent_bounded_queue,11:   1,213,040 msg/sec (mean:     961,922 stdev:     109,544) msg/sec
tbb::concurrent_bounded_queue,12:   1,157,905 msg/sec (mean:     990,672 stdev:     100,562) msg/sec
tbb::concurrent_bounded_queue,13:   1,321,028 msg/sec (mean:   1,077,006 stdev:     115,641) msg/sec
                   AtomicQueue,1:   9,759,971 msg/sec (mean:   8,289,720 stdev:   1,217,898) msg/sec
                   AtomicQueue,2:   5,551,276 msg/sec (mean:   4,422,193 stdev:     468,885) msg/sec
                   AtomicQueue,3:   3,937,683 msg/sec (mean:   3,378,673 stdev:     265,828) msg/sec
                   AtomicQueue,4:   3,492,101 msg/sec (mean:   2,686,387 stdev:     428,926) msg/sec
                   AtomicQueue,5:   2,650,348 msg/sec (mean:   2,248,884 stdev:     239,093) msg/sec
                   AtomicQueue,6:   2,492,281 msg/sec (mean:   2,038,296 stdev:     238,984) msg/sec
                   AtomicQueue,7:   1,304,372 msg/sec (mean:   1,127,092 stdev:      99,367) msg/sec
                   AtomicQueue,8:   1,083,301 msg/sec (mean:     926,286 stdev:     107,203) msg/sec
                   AtomicQueue,9:   1,013,039 msg/sec (mean:     853,978 stdev:     106,103) msg/sec
                  AtomicQueue,10:   1,071,124 msg/sec (mean:     830,858 stdev:     126,075) msg/sec
                  AtomicQueue,11:   1,040,391 msg/sec (mean:     812,820 stdev:     119,143) msg/sec
                  AtomicQueue,12:   1,079,620 msg/sec (mean:     816,151 stdev:     141,325) msg/sec
                  AtomicQueue,13:   1,256,532 msg/sec (mean:     857,250 stdev:     188,958) msg/sec
           BlockingAtomicQueue,1:  68,666,934 msg/sec (mean:  49,140,572 stdev:  18,686,875) msg/sec
           BlockingAtomicQueue,2:  17,451,686 msg/sec (mean:  11,502,455 stdev:   2,658,769) msg/sec
           BlockingAtomicQueue,3:  17,462,525 msg/sec (mean:  13,084,148 stdev:   2,764,593) msg/sec
           BlockingAtomicQueue,4:  20,015,414 msg/sec (mean:  15,096,382 stdev:   2,692,777) msg/sec
           BlockingAtomicQueue,5:  21,160,763 msg/sec (mean:  15,833,361 stdev:   3,401,577) msg/sec
           BlockingAtomicQueue,6:  21,323,176 msg/sec (mean:  16,231,944 stdev:   3,415,167) msg/sec
           BlockingAtomicQueue,7:  17,352,048 msg/sec (mean:  11,075,951 stdev:   2,716,966) msg/sec
           BlockingAtomicQueue,8:  16,279,532 msg/sec (mean:  11,067,140 stdev:   2,954,936) msg/sec
           BlockingAtomicQueue,9:  15,683,801 msg/sec (mean:  10,910,722 stdev:   2,865,635) msg/sec
          BlockingAtomicQueue,10:  16,975,831 msg/sec (mean:  11,288,098 stdev:   3,376,545) msg/sec
          BlockingAtomicQueue,11:  16,986,634 msg/sec (mean:  11,263,550 stdev:   3,449,288) msg/sec
          BlockingAtomicQueue,12:  17,341,962 msg/sec (mean:  11,253,321 stdev:   3,427,861) msg/sec
          BlockingAtomicQueue,13:  21,286,526 msg/sec (mean:  12,243,851 stdev:   5,090,129) msg/sec
                  AtomicQueue2,1:   8,411,912 msg/sec (mean:   6,592,580 stdev:     989,437) msg/sec
                  AtomicQueue2,2:   4,776,622 msg/sec (mean:   4,177,573 stdev:     321,332) msg/sec
                  AtomicQueue2,3:   3,452,154 msg/sec (mean:   3,177,045 stdev:     237,084) msg/sec
                  AtomicQueue2,4:   3,139,269 msg/sec (mean:   2,583,758 stdev:     271,817) msg/sec
                  AtomicQueue2,5:   2,855,631 msg/sec (mean:   2,233,939 stdev:     265,530) msg/sec
                  AtomicQueue2,6:   2,403,095 msg/sec (mean:   1,960,660 stdev:     220,065) msg/sec
                  AtomicQueue2,7:   1,368,201 msg/sec (mean:   1,123,255 stdev:     122,034) msg/sec
                  AtomicQueue2,8:   1,061,715 msg/sec (mean:     911,566 stdev:     109,810) msg/sec
                  AtomicQueue2,9:   1,066,555 msg/sec (mean:     845,567 stdev:     123,918) msg/sec
                 AtomicQueue2,10:   1,055,948 msg/sec (mean:     833,438 stdev:     127,311) msg/sec
                 AtomicQueue2,11:   1,118,051 msg/sec (mean:     818,161 stdev:     149,693) msg/sec
                 AtomicQueue2,12:   1,068,713 msg/sec (mean:     823,639 stdev:     153,354) msg/sec
                 AtomicQueue2,13:   1,078,537 msg/sec (mean:     861,185 stdev:     155,784) msg/sec
          BlockingAtomicQueue2,1:  39,681,843 msg/sec (mean:  17,274,996 stdev:  10,063,758) msg/sec
          BlockingAtomicQueue2,2:  14,543,387 msg/sec (mean:  10,099,061 stdev:   2,843,547) msg/sec
          BlockingAtomicQueue2,3:  18,757,837 msg/sec (mean:  11,923,479 stdev:   2,965,424) msg/sec
          BlockingAtomicQueue2,4:  19,715,309 msg/sec (mean:  13,879,254 stdev:   3,116,542) msg/sec
          BlockingAtomicQueue2,5:  20,939,894 msg/sec (mean:  14,058,602 stdev:   3,978,787) msg/sec
          BlockingAtomicQueue2,6:  20,621,634 msg/sec (mean:  14,037,438 stdev:   3,676,946) msg/sec
          BlockingAtomicQueue2,7:  12,647,499 msg/sec (mean:   9,992,268 stdev:   1,509,709) msg/sec
          BlockingAtomicQueue2,8:  15,973,301 msg/sec (mean:   9,964,540 stdev:   3,130,516) msg/sec
          BlockingAtomicQueue2,9:  15,591,109 msg/sec (mean:  10,102,342 stdev:   3,409,520) msg/sec
         BlockingAtomicQueue2,10:  16,220,540 msg/sec (mean:  10,419,866 stdev:   3,767,179) msg/sec
         BlockingAtomicQueue2,11:  16,370,119 msg/sec (mean:  10,647,052 stdev:   3,986,337) msg/sec
         BlockingAtomicQueue2,12:  18,422,462 msg/sec (mean:  10,758,590 stdev:   4,505,264) msg/sec
         BlockingAtomicQueue2,13:  17,183,451 msg/sec (mean:  10,493,666 stdev:   4,169,898) msg/sec
```
