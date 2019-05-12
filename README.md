# atomic_queue
Multiple producer multipe consumer C++ lock-free queues. They contain busy loops, so they are not wait-free.
Work in progress.
Available containers are:
* `AtomicQueue` - a fixed size ring-buffer for atomic elements.
* `BlockingAtomicQueue`  - a faster fixed size ring-buffer for atomic elements which busy-waits when empty or full.
* `AtomicQueue2` - a fixed size ring-buffer for non-atomic elements.
* `BlockingAtomicQueue2`  - a faster fixed size ring-buffer for non-atomic elements which busy-waits when empty or full.

# Instructions
```
cd atomic_queue
make -r run_benchmarks
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
* More benchmarks.
* Documentation.
