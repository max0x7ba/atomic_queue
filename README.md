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
## Ping-pong benchmark
One thread posts an integer to another thread and waits for the reply using two queues. The benchmarks measures the total time of 1,000,000 ping-pongs, best of 10 runs. Contention is minimal here to be able to achieve and measure the lowest latency. Reports the total time and the average round-trip time. Wait-free `boost::lockfree::spsc_queue` and a pthread_spinlock-based queue are used as reference benchmarks.

Results on Intel Core i7-7700K, Ubuntu 18.04.2 LTS (all times are in seconds):
```
Running latency and throughput benchmarks...
         AtomicQueue: Producers | Consumers: Time: 0.040885450 | 0.040965850. Latency min/avg/max: 0.000000017/0.000000082/0.000019508 | 0.000000104/0.000305488/0.000560653.
 BlockingAtomicQueue: Producers | Consumers: Time: 0.021066340 | 0.023597608. Latency min/avg/max: 0.000000016/0.000000042/0.000002959 | 0.000000002/0.000020129/0.000285901.
        AtomicQueue2: Producers | Consumers: Time: 0.048778506 | 0.052850210. Latency min/avg/max: 0.000000018/0.000000098/0.000004132 | 0.000000042/0.000000227/0.000005552.
BlockingAtomicQueue2: Producers | Consumers: Time: 0.039384076 | 0.040031753. Latency min/avg/max: 0.000000025/0.000000079/0.000002374 | 0.000000075/0.000000126/0.000002426.
    pthread_spinlock: Producers | Consumers: Time: 0.042513058 | 0.056737730. Latency min/avg/max: 0.000000011/0.000000085/0.000051612 | 0.000000021/0.000311343/0.000783453.
Running ping-pong benchmarks...
   boost::spsc_queue: Time: 0.116934172. Round trip time: 0.000000117.
         AtomicQueue: Time: 0.149763958. Round trip time: 0.000000150.
 BlockingAtomicQueue: Time: 0.128459128. Round trip time: 0.000000128.
        AtomicQueue2: Time: 0.180772725. Round trip time: 0.000000181.
BlockingAtomicQueue2: Time: 0.176750451. Round trip time: 0.000000177.
    pthread_spinlock: Time: 0.268714511. Round trip time: 0.000000269.

```
# TODO
* More benchmarks.
* Documentation.
