/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "atomic_queue.h"
#include "atomic_queue_mutex.h"
#include "barrier.h"
#include "cpu_base_frequency.h"
#include "huge_pages.h"
#include "moodycamel.h"

#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include <tbb/concurrent_queue.h>
#include <tbb/spin_mutex.h>

#include <algorithm>
#include <clocale>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using std::uint64_t;

using namespace ::atomic_queue;

template<class T>
using Type = std::common_type<T>; // Similar to boost::type<>.

namespace {

using sum_t = long long;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double const TSC_TO_SECONDS = 1e-9 / cpu_base_frequency();

template<class T>
inline double to_seconds(T tsc) {
    return tsc * TSC_TO_SECONDS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
struct BoostSpScAdapter : Queue {
    using T = typename Queue::value_type;

    void push(T element) {
        while(!this->Queue::push(element))
            spin_loop_pause();
    }

    T pop() {
        T element;
        while(!this->Queue::pop(element))
            spin_loop_pause();
        return element;
    }
};

template<class Queue>
struct BoostQueueAdapter : BoostSpScAdapter<Queue> {
    using T = typename Queue::value_type;

    void push(T element) {
        while(!this->Queue::bounded_push(element))
            spin_loop_pause();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue, size_t Capacity>
struct TbbAdapter : RetryDecorator<Queue> {
    TbbAdapter() {
        this->set_capacity(Capacity);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue, size_t Capacity>
struct CapacityToConstructor : Queue {
    CapacityToConstructor()
        : Queue(Capacity) {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
void throughput_producer(unsigned N, Queue* queue, std::atomic<uint64_t>* t0, Barrier* barrier, unsigned cpu) {
    set_thread_affinity(cpu);
    barrier->wait();
    // The first producer saves the start time.
    uint64_t expected = 0;
    t0->compare_exchange_strong(expected, __builtin_ia32_rdtsc(), std::memory_order_acq_rel, std::memory_order_relaxed);

    for(unsigned n = 1, stop = N + 1; n <= stop; ++n)
        queue->push(n);
}

template<class Queue>
void throughput_consumer_impl(unsigned N, Queue* queue, sum_t* consumer_sum, std::atomic<unsigned>* last_consumer, uint64_t* t1) {
    unsigned const stop = N + 1;
    sum_t sum = 0;

    for(;;) {
        unsigned n = queue->pop();
        if(n == stop)
            break;
        sum += n;
    }

    // The last consumer saves the end time.
    auto t = __builtin_ia32_rdtsc();
    if(1 == last_consumer->fetch_sub(1, std::memory_order_acq_rel))
        *t1 = t;

    *consumer_sum = sum;
}

template<class Queue>
void throughput_consumer(unsigned N, Queue* queue, sum_t* consumer_sum, std::atomic<unsigned>* last_consumer, uint64_t* t1, Barrier* barrier, unsigned cpu) {
    set_thread_affinity(cpu);
    barrier->wait();
    throughput_consumer_impl(N, queue, consumer_sum, last_consumer, t1);
}

template<class Queue>
uint64_t benchmark_throughput(HugePages& hp, std::vector<unsigned> const& hw_thread_ids, unsigned N, unsigned thread_count, bool alternative_placement, sum_t* consumer_sums) {
    set_thread_affinity(hw_thread_ids.back()); // Use this thread for the last consumer.
    unsigned cpu_idx = 0;

    auto queue = hp.create_unique_ptr<Queue>();
    std::atomic<uint64_t> t0{0};
    uint64_t t1 = 0;
    std::atomic<unsigned> last_consumer{thread_count};

    Barrier barrier;
    std::vector<std::thread> threads(thread_count * 2 - 1);
    if(alternative_placement) {
        for(unsigned i = 0; i < thread_count; ++i) {
            threads[i] = std::thread(throughput_producer<Queue>, N, queue.get(), &t0, &barrier, hw_thread_ids[cpu_idx++]);
            if(i != thread_count - 1) // This thread is the last consumer.
                threads[thread_count + i] = std::thread(throughput_consumer<Queue>, N, queue.get(), consumer_sums + i, &last_consumer, &t1, &barrier, hw_thread_ids[cpu_idx++]);
        }
    } else {
        for(unsigned i = 0; i < thread_count; ++i)
            threads[i] = std::thread(throughput_producer<Queue>, N, queue.get(), &t0, &barrier, hw_thread_ids[cpu_idx++]);
        for(unsigned i = 0; i < thread_count - 1; ++i) // This thread is the last consumer.
            threads[thread_count + i] = std::thread(throughput_consumer<Queue>, N, queue.get(), consumer_sums + i, &last_consumer, &t1, &barrier, hw_thread_ids[cpu_idx++]);
    }

    barrier.release(thread_count * 2 - 1);
    throughput_consumer_impl(N, queue.get(), consumer_sums + (thread_count - 1), &last_consumer, &t1); // Use this thread for the last consumer.

    for(auto& t : threads)
        t.join();

    return t1 - t0.load(std::memory_order_relaxed);
}

template<class Queue>
void run_throughput_benchmark(char const* name, HugePages& hp, std::vector<unsigned> const& hw_thread_ids, unsigned M, unsigned thread_count_min, unsigned thread_count_max) {
    int constexpr RUNS = 3;
    std::vector<sum_t> consumer_sums(thread_count_max);

    for(bool alternative_placement : {false, true}) {
        for(unsigned threads = thread_count_min; threads <= thread_count_max; ++threads) {
            unsigned const N = M / threads;

            sum_t const expected_sum = (N + 1) / 2. * N;
            double const expected_sum_inv = 1. / expected_sum;

            uint64_t min_time = std::numeric_limits<uint64_t>::max();
            for(unsigned run = RUNS; run--;) {
                uint64_t time = benchmark_throughput<Queue>(hp, hw_thread_ids, N, threads, alternative_placement, consumer_sums.data());
                min_time = std::min(min_time, time);

                // Calculate the checksum.
                sum_t total_sum = 0;
                for(unsigned i = 0; i < threads; ++i) {
                    auto consumer_sum = consumer_sums[i];
                    total_sum += consumer_sum;

                    // Verify that no consumer was starved.
                    auto consumer_sum_frac = consumer_sum * expected_sum_inv;
                    if(consumer_sum_frac < .1)
                        std::fprintf(stderr, "%s: producers: %u: consumer %u received too few messages: %.2lf%% of expected.\n", name, threads, i,
                                     consumer_sum_frac);
                }

                // Verify that all messages were received exactly once: no duplicates, no omissions.
                if(auto diff = total_sum - expected_sum * threads)
                    std::fprintf(stderr, "%s: wrong checksum error: producers: %u, expected_sum: %'lld, diff: %'lld.\n", name, threads, expected_sum * threads,
                                 diff);
            }

            double min_seconds = to_seconds(min_time);
            unsigned msg_per_sec = N * threads / min_seconds;
            std::printf("%32s,%2u,%c: %'11u msg/sec\n", name, threads, alternative_placement ? 'i' : 's', msg_per_sec);
        }
    }
}

template<class Queue>
void run_throughput_benchmark(char const* name, HugePages& hp, std::vector<unsigned> const& hw_thread_ids, Type<Queue>, unsigned thread_count_min = 1) {
    unsigned const thread_count_max = hw_thread_ids.size() / 2;
    run_throughput_benchmark<Queue>(name, hp, hw_thread_ids, 1000000, thread_count_min, thread_count_max);
}

template<class... Args>
void run_throughput_spsc_benchmark(char const* name, HugePages& hp, std::vector<unsigned> const& hw_thread_ids, Type<BoostSpScAdapter<boost::lockfree::spsc_queue<Args...>>>) {
    using Queue = BoostSpScAdapter<boost::lockfree::spsc_queue<Args...>>;
    run_throughput_benchmark<Queue>(name, hp, hw_thread_ids, 1000000, 1, 1); // spsc_queue can only handle 1 producer and 1 consumer.
}

template<class Queue>
void run_throughput_spsc_benchmark(char const* name, HugePages& hp, std::vector<unsigned> const& hw_thread_ids, Type<Queue>) {
    run_throughput_benchmark<Queue>(name, hp, hw_thread_ids, 1000000, 1, 1); // ReaderWriterQueue can only handle 1 producer and 1 consumer.
}

void run_throughput_benchmarks(HugePages& hp, std::vector<unsigned> const& hw_thread_ids) {
    std::printf("---- Running throughput benchmarks (higher is better) ----\n");

    int constexpr CAPACITY = 65536;

    run_throughput_spsc_benchmark("boost::lockfree::spsc_queue", hp, hw_thread_ids,
                                  Type<BoostSpScAdapter<boost::lockfree::spsc_queue<unsigned, boost::lockfree::capacity<CAPACITY>>>>{});
    run_throughput_benchmark("boost::lockfree::queue", hp, hw_thread_ids, Type<BoostQueueAdapter<boost::lockfree::queue<unsigned, boost::lockfree::capacity<CAPACITY - 2>>>>{});

    run_throughput_benchmark("pthread_spinlock", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueueSpinlock<unsigned, CAPACITY>>>{});
    // run_throughput_benchmark("FairSpinlock", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueueMutex<unsigned, CAPACITY, FairSpinlock>>>{});
    // run_throughput_benchmark("UnfairSpinlock", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueueMutex<unsigned, CAPACITY, UnfairSpinlock>>>{});

    run_throughput_spsc_benchmark("moodycamel::ReaderWriterQueue", hp, hw_thread_ids, Type<MoodyCamelReaderWriterQueue<unsigned, CAPACITY>>{});
    run_throughput_benchmark("moodycamel::ConcurrentQueue", hp, hw_thread_ids, Type<MoodyCamelQueue<unsigned, CAPACITY>>{});

    run_throughput_benchmark("tbb::spin_mutex", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueueMutex<unsigned, CAPACITY, tbb::spin_mutex>>>{});
    run_throughput_benchmark("tbb::speculative_spin_mutex", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueueMutex<unsigned, CAPACITY, tbb::speculative_spin_mutex>>>{});
    run_throughput_benchmark("tbb::concurrent_bounded_queue", hp, hw_thread_ids, Type<TbbAdapter<tbb::concurrent_bounded_queue<unsigned>, CAPACITY>>{});

    // Enable MINIMIZE_CONTENTION for 2 or more producers/consumers.
    run_throughput_spsc_benchmark("AtomicQueue", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueue<unsigned, CAPACITY, 0u, false>>>{});
    run_throughput_benchmark("AtomicQueue", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueue<unsigned, CAPACITY, 0u, true>>>{}, 2);

    run_throughput_benchmark("AtomicQueueB", hp, hw_thread_ids, Type<RetryDecorator<CapacityToConstructor<AtomicQueueB<unsigned>, CAPACITY>>>{});

    // Enable MINIMIZE_CONTENTION for 2 or more producers/consumers.
    run_throughput_spsc_benchmark("OptimistAtomicQueue", hp, hw_thread_ids, Type<AtomicQueue<unsigned, CAPACITY, 0u, false>>{});
    run_throughput_benchmark("OptimistAtomicQueue", hp, hw_thread_ids, Type<AtomicQueue<unsigned, CAPACITY, 0u, true>>{}, 2);

    run_throughput_benchmark("OptimistAtomicQueueB", hp, hw_thread_ids, Type<CapacityToConstructor<AtomicQueueB<unsigned>, CAPACITY>>{});

    // Enable MINIMIZE_CONTENTION for 2 or more producers/consumers.
    run_throughput_spsc_benchmark("AtomicQueue2", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueue2<unsigned, CAPACITY, false>>>{});
    run_throughput_benchmark("AtomicQueue2", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueue2<unsigned, CAPACITY, true>>>{}, 2);

    run_throughput_benchmark("AtomicQueueB2", hp, hw_thread_ids, Type<RetryDecorator<CapacityToConstructor<AtomicQueueB2<unsigned>, CAPACITY>>>{});

    // Enable MINIMIZE_CONTENTION for 2 or more producers/consumers.
    run_throughput_spsc_benchmark("OptimistAtomicQueue2", hp, hw_thread_ids, Type<AtomicQueue2<unsigned, CAPACITY, false>>{});
    run_throughput_benchmark("OptimistAtomicQueue2", hp, hw_thread_ids, Type<AtomicQueue2<unsigned, CAPACITY, true>>{}, 2);

    run_throughput_benchmark("OptimistAtomicQueueB2", hp, hw_thread_ids, Type<CapacityToConstructor<AtomicQueueB2<unsigned>, CAPACITY>>{});

    // run_throughput_benchmark<RetryDecorator<AtomicQueueSpinlockHle<unsigned, CAPACITY>>>("SpinlockHle");

    std::printf("\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue, bool Sender>
void ping_pong_thread_impl(Queue* q1, Queue* q2, unsigned N, uint64_t* time) {
    uint64_t t0 = __builtin_ia32_rdtsc();
    for(unsigned i = 1, j = 0; j < N; ++i) {
        if(Sender) {
            q1->push(i);
            j = q2->pop();
        } else {
            j = q1->pop();
            q2->push(i);
        }
    }
    uint64_t t1 = __builtin_ia32_rdtsc();

    *time = t1 - t0;
}

template<class Queue>
inline void ping_pong_thread_receiver(Barrier* barrier, Queue* q1, Queue* q2, unsigned N, uint64_t* time, unsigned cpu) {
    set_thread_affinity(cpu);
    barrier->wait();
    ping_pong_thread_impl<Queue, false>(q1, q2, N, time);
}

template<class Queue>
inline void ping_pong_thread_sender(Barrier* barrier, Queue* q1, Queue* q2, unsigned N, uint64_t* time, unsigned cpu) {
    set_thread_affinity(cpu);
    barrier->release(1);
    ping_pong_thread_impl<Queue, true>(q1, q2, N, time);
}

template<class Queue>
inline std::array<uint64_t, 2> ping_pong_benchmark(unsigned N, HugePages& hp, unsigned const(&cpus)[2]) {
    auto q1 = hp.create_unique_ptr<Queue>();
    auto q2 = hp.create_unique_ptr<Queue>();
    Barrier barrier;
    std::array<uint64_t, 2> times;
    std::thread receiver(ping_pong_thread_receiver<Queue>, &barrier, q1.get(), q2.get(), N, &times[0], cpus[1]);
    ping_pong_thread_sender<Queue>(&barrier, q1.get(), q2.get(), N, &times[1], cpus[0]);
    receiver.join();
    return times;
}

template<class Queue>
void run_ping_pong_benchmark(char const* name, HugePages& hp, std::vector<unsigned> const& hw_thread_ids) {
    int constexpr N = 100000;
    int constexpr RUNS = 10;

    unsigned const cpus[2] = {hw_thread_ids[0], hw_thread_ids[1]}; // With HT enabled this selects 2 threads on the same core.

    // Select the best of RUNS runs.
    std::array<uint64_t, 2> best_times = {std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::max()};
    for(unsigned run = RUNS; run--;) {
        auto times = ping_pong_benchmark<Queue>(N, hp, cpus);
        if(best_times[0] + best_times[1] > times[0] + times[1])
            best_times = times;

        if(!hp.empty()) {
            std::fprintf(stderr, "%s: %zu bytes of HugePages memory leaked.\n", name, hp.used());
            hp.reset();
        }
    }

    auto avg_time = to_seconds((best_times[0] + best_times[1]) / 2);
    auto round_trip_time = avg_time / N;
    std::printf("%32s: %.9f sec/round-trip\n", name, round_trip_time);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void run_ping_pong_benchmarks(HugePages& hp, std::vector<unsigned> const& hw_thread_ids) {
    std::printf("---- Running ping-pong benchmarks (lower is better) ----\n");

    // This benchmarks doesn't require queue capacity greater than 1, however, capacity of 1 elides
    // some instructions completely because of (x % 1) is always 0. Use something greater than 1 to
    // preclude aggressive optimizations.
    constexpr unsigned CAPACITY = 8;

    run_ping_pong_benchmark<BoostSpScAdapter<boost::lockfree::spsc_queue<unsigned, boost::lockfree::capacity<CAPACITY>>>>("boost::lockfree::spsc_queue", hp, hw_thread_ids);
    run_ping_pong_benchmark<BoostQueueAdapter<boost::lockfree::queue<unsigned, boost::lockfree::capacity<CAPACITY>>>>("boost::lockfree::queue", hp, hw_thread_ids);

    run_ping_pong_benchmark<RetryDecorator<AtomicQueueSpinlock<unsigned, CAPACITY>>>("pthread_spinlock", hp, hw_thread_ids);
    // run_ping_pong_benchmark<RetryDecorator<AtomicQueueMutex<unsigned, CAPACITY, FairSpinlock>>>("FairSpinlock", hp, hw_thread_ids);
    // run_ping_pong_benchmark<RetryDecorator<AtomicQueueMutex<unsigned, CAPACITY, UnfairSpinlock>>>("UnfairSpinlock", hp, hw_thread_ids);

    run_ping_pong_benchmark<MoodyCamelReaderWriterQueue<unsigned, CAPACITY>>("moodycamel::ReaderWriterQueue", hp, hw_thread_ids);
    run_ping_pong_benchmark<MoodyCamelQueue<unsigned, CAPACITY>>("moodycamel::ConcurrentQueue", hp, hw_thread_ids);

    run_ping_pong_benchmark<RetryDecorator<AtomicQueueMutex<unsigned, CAPACITY, tbb::spin_mutex>>>("tbb::spin_mutex", hp, hw_thread_ids);
    run_ping_pong_benchmark<RetryDecorator<AtomicQueueMutex<unsigned, CAPACITY, tbb::speculative_spin_mutex>>>("tbb::speculative_spin_mutex", hp, hw_thread_ids);
    run_ping_pong_benchmark<TbbAdapter<tbb::concurrent_bounded_queue<unsigned>, CAPACITY>>("tbb::concurrent_bounded_queue", hp, hw_thread_ids);

    // Use MAXIMIZE_THROUGHPUT=false for better latency.
    using A = HugePageAllocator<unsigned>;
    constexpr bool MT = false; // MAXIMIZE_THROUGHPUT
    constexpr bool MC = true; // MINIMIZE_CONTENTION
    run_ping_pong_benchmark<RetryDecorator<AtomicQueue<unsigned, CAPACITY, 0u, MC, MT>>>("AtomicQueue", hp, hw_thread_ids);
    run_ping_pong_benchmark<RetryDecorator<CapacityToConstructor<AtomicQueueB<unsigned, A, 0u, MT>, CAPACITY>>>("AtomicQueueB", hp, hw_thread_ids);
    run_ping_pong_benchmark<AtomicQueue<unsigned, CAPACITY, 0u, MC, MT>>("OptimistAtomicQueue", hp, hw_thread_ids);
    run_ping_pong_benchmark<CapacityToConstructor<AtomicQueueB<unsigned, A, 0u, MT>, CAPACITY>>("OptimistAtomicQueueB", hp, hw_thread_ids);
    run_ping_pong_benchmark<RetryDecorator<AtomicQueue2<unsigned, CAPACITY, MC, MT>>>("AtomicQueue2", hp, hw_thread_ids);
    run_ping_pong_benchmark<RetryDecorator<CapacityToConstructor<AtomicQueueB2<unsigned, A, MT>, CAPACITY>>>("AtomicQueueB2", hp, hw_thread_ids);
    run_ping_pong_benchmark<AtomicQueue2<unsigned, CAPACITY, MC, MT>>("OptimistAtomicQueue2", hp, hw_thread_ids);
    run_ping_pong_benchmark<CapacityToConstructor<AtomicQueueB2<unsigned, A, MT>, CAPACITY>>("OptimistAtomicQueueB2", hp, hw_thread_ids);

    // run_ping_pong_benchmark<RetryDecorator<AtomicQueueSpinlockHle<unsigned, CAPACITY>>>("SpinlockHle");

    std::printf("\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main() {
    std::setlocale(LC_NUMERIC, "");

    size_t constexpr GB = 1024u * 1024 * 1024;
    HugePages hp(HugePages::PAGE_1GB, 1 * GB); // Allocate one 1GB huge page to minimize TLB misses.
    HugePageAllocatorBase::hp = &hp;

    auto hw_thread_ids = sort_hw_threads_by_core_id(get_cpu_topology_info());
    if(hw_thread_ids.size() < 2)
        throw std::runtime_error("A CPU with at least 2 hardware threads is required.");

    run_throughput_benchmarks(hp, hw_thread_ids);
    run_ping_pong_benchmarks(hp, hw_thread_ids);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
