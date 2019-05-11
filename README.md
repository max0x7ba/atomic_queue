# atomic_queue
Multiple producer multipe consumer C++ lock-free queue.
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
One thread posts an integer to another thread and waits for the reply using two queues. The benchmarks measures the total time of 1,000,000 ping-pongs. Contention is minimal here to be able to achieve and measure the lowest latency. Reports the total time and the average round-trip time.

Results on Intel Core i7-7700K, Ubuntu 18.04.2 LTS:
```
         AtomicQueue: 0.136999676 seconds. Round trip time: 0.000000137 seconds. Difference: -0.000000055 seconds.
 BlockingAtomicQueue: 0.130360431 seconds. Round trip time: 0.000000130 seconds. Difference: -0.000000016 seconds.
        AtomicQueue2: 0.168053783 seconds. Round trip time: 0.000000168 seconds. Difference: -0.000000079 seconds.
BlockingAtomicQueue2: 0.173906307 seconds. Round trip time: 0.000000174 seconds. Difference: -0.000000059 seconds.

```
# TODO
* More benchmarks.
* Benchmarks that compares all variants automatically.
* Documentation.
