# atomic_queue
Multiple producer multipe consumer C++ *lock-free* queues. They contain busy loops, so they are not *wait-free*.
Work in progress.
Available containers are:
* `AtomicQueue` - a fixed size ring-buffer for atomic elements.
* `BlockingAtomicQueue` - a faster fixed size ring-buffer for atomic elements which busy-waits when empty or full.
* `AtomicQueue2` - a fixed size ring-buffer for non-atomic elements.
* `BlockingAtomicQueue2` - a faster fixed size ring-buffer for non-atomic elements which busy-waits when empty or full.
* `pthread_spinlock` - a fixed size ring-buffer for non-atomic elements, uses `pthread_spinlock_t` for locking.
* `SpinlockHle` - a fixed size ring-buffer for non-atomic elements, uses a spinlock with Intel Hardware Lock Elision (only when compiling with gcc).

# Notes

In a real-world multiple-producer-multiple-consumer scenario the ring-buffer size should be set to the maximum allowable queue size. When the buffer size is execeeded it means that the consumers cannot consume the elements fast enough, fixing which would require either of:

* increasing the buffer size to be able to handle spikes of produced elements, or
* increasing the number of consumers, or
* decreasing the number of producers.

All the available queues here use a ring-buffer array for storing queue elements.

Using a power-of-2 ring-buffer array size allows for a couple of important optimizations:

* The writer and reader indexes get mapped into the ring-buffer array index using modulo `% SIZE` binary operator (division and modulo are some of the most expensive operations). The array size `SIZE` is fixed at the compile time, so that the compiler may be able to turn the modulo operator into an assembly block of less expensive instructions. However, a power-of-2 size turns that modulo operator into one plain `and` instruction and that is as fast as it gets.
* The *element index within the cache line* gets swapped with the *cache line index* within the *ring-buffer array element index*, so that logically subsequent elements reside in different cache lines. This eliminates contention between producers and consumers on the ring-buffer cache lines. Instead of N producers together with M consumers competing on the same ring-buffer array cache line in the worst case, it is only one producer competing with one comsumer.

In other words, power-of-2 ring-buffer array size yeilds top performance.

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

# Available Benchmarks
Benchmarks report times in seconds.
## Latency and throughput benchmark
Two producer threads post into one queue, two consumer threads drain the queue. Producer and consumer total times are measured. Producer latencies are times it takes to post one item. Consumer latencies are the time it took the item to arrive. These times include the price of rdtsc instruction.

Results on Intel Core i7-7700K, Ubuntu 18.04.2 LTS (all times are in seconds):
```
         AtomicQueue: Producers | Consumers: Time: 0.047221624 | 0.047307788. Latency min/avg/max: 0.000000017/0.000000094/0.000002508 | 0.000000033/0.000000176/0.000002485.
 BlockingAtomicQueue: Producers | Consumers: Time: 0.021818387 | 0.025183147. Latency min/avg/max: 0.000000015/0.000000044/0.000002541 | 0.000000001/0.000002154/0.000129812.
        AtomicQueue2: Producers | Consumers: Time: 0.042883262 | 0.044135063. Latency min/avg/max: 0.000000018/0.000000086/0.000004860 | 0.000000051/0.000000138/0.000005022.
BlockingAtomicQueue2: Producers | Consumers: Time: 0.037858634 | 0.038143659. Latency min/avg/max: 0.000000017/0.000000076/0.000002191 | 0.000000047/0.000000126/0.000002425.
    pthread_spinlock: Producers | Consumers: Time: 0.041262248 | 0.059916213. Latency min/avg/max: 0.000000011/0.000000083/0.000048237 | 0.000000019/0.000193690/0.000808207.
```
## Ping-pong benchmark
One thread posts an integer to another thread and waits for the reply using two queues. The benchmarks measures the total time of 1,000,000 ping-pongs, best of 10 runs. Contention is minimal here to be able to achieve and measure the lowest latency. Reports the total time and the average round-trip time. Wait-free `boost::lockfree::spsc_queue` and a pthread_spinlock-based queue are used as reference benchmarks.

Results on Intel Core i7-7700K, Ubuntu 18.04.2 LTS (all times are in seconds):
```
   boost::spsc_queue: Time: 0.119852952. Round trip time: 0.000000120.
         AtomicQueue: Time: 0.140082919. Round trip time: 0.000000140.
 BlockingAtomicQueue: Time: 0.123989804. Round trip time: 0.000000124.
        AtomicQueue2: Time: 0.180023592. Round trip time: 0.000000180.
BlockingAtomicQueue2: Time: 0.136005360. Round trip time: 0.000000136.
    pthread_spinlock: Time: 0.296041086. Round trip time: 0.000000296.
```
# TODO
* CMake.
* More benchmarks.
* More Documentation.
