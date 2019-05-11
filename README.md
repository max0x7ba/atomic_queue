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
## Ping-pong benchmark
One thread posts an integer to another thread and waits for the reply using two queues. The benchmarks measures the total time of 1,000,000 ping-pongs, best of 10 runs. Contention is minimal here to be able to achieve and measure the lowest latency. Reports the total time and the average round-trip time. Wait-free `boost::lockfree::spsc_queue` and a pthread_spinlock-based queue are used as reference benchmarks.

Results on Intel Core i7-7700K, Ubuntu 18.04.2 LTS:
```
   boost::spsc_queue: 0.118124462 seconds. Round trip time: 0.000000118 seconds. Difference: -0.000000047 seconds.
         AtomicQueue: 0.145098411 seconds. Round trip time: 0.000000145 seconds. Difference: -0.000000039 seconds.
 BlockingAtomicQueue: 0.120760856 seconds. Round trip time: 0.000000121 seconds. Difference: -0.000000015 seconds.
        AtomicQueue2: 0.178867058 seconds. Round trip time: 0.000000179 seconds. Difference: -0.000000051 seconds.
BlockingAtomicQueue2: 0.173858355 seconds. Round trip time: 0.000000174 seconds. Difference: -0.000000041 seconds.
  pthread_spinlock_t: 0.851772067 seconds. Round trip time: 0.000000852 seconds. Difference: -0.000000090 seconds.

```
# TODO
* More benchmarks.
* Benchmarks that compares all variants automatically.
* Documentation.
