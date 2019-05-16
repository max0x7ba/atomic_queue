/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#include "atomic_queue_spin_lock.h"
#include "cpu_base_frequency.h"
#include "atomic_queue.h"
#include "barrier.h"

#include <boost/lockfree/spsc_queue.hpp>
#include <boost/lockfree/queue.hpp>

#include <algorithm>
#include <stdexcept>
#include <clocale>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <future>
#include <limits>
#include <vector>

#include <emmintrin.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using std::uint64_t;

using namespace ::atomic_queue;

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double const CPU_FREQ = cpu_base_frequency();

template<class T>
inline double to_seconds(T tsc) {
    return tsc / CPU_FREQ * 1e-9;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
struct BoostAdapter : Queue {
    using T = typename Queue::value_type;

    using Queue::Queue;

    void push(T element) {
        while(!this->Queue::push(element))
            _mm_pause();
    }

    T pop() {
        T element;
        while(!this->Queue::pop(element))
            _mm_pause();
        return element;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Stopper {
    std::atomic<unsigned> producer_count_;
    unsigned const consumer_count_;

    template<class Queue>
    void operator()(Queue* const queue, unsigned stop) {
        if(1 == producer_count_.fetch_sub(1, std::memory_order_relaxed)) {
            for(unsigned i = consumer_count_; i--;)
                queue->push(stop);
        }
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
void throughput_producer(unsigned N, Queue* queue, Barrier* barrier, Stopper* stopper) {
    unsigned const stop = N + 1;

    barrier->wait();

    for(unsigned n = N; n; --n)
        queue->push(n);

    (*stopper)(queue, stop);
}

template<class Queue>
void throughput_consumer(unsigned N, Queue* queue, Barrier* barrier) {
    unsigned const stop = N + 1;

    barrier->wait();

    while(stop != queue->pop())
        ;
}

template<class Queue>
uint64_t benchmark_throughput(unsigned N, unsigned producer_count, unsigned consumer_count) {
    Queue queue;
    Stopper stopper{{producer_count}, consumer_count};

    Barrier barrier;
    std::vector<std::thread> threads(producer_count + consumer_count);
    for(unsigned i = 0; i < producer_count; ++i)
        threads[i] = std::thread(throughput_producer<Queue>, N, &queue, &barrier, &stopper);
    for(unsigned i = 0; i < consumer_count; ++i)
        threads[producer_count + i] = std::thread(throughput_consumer<Queue>, N, &queue, &barrier);

    auto t0 = __builtin_ia32_rdtsc();
    barrier.release(producer_count + consumer_count);
    for(auto& t: threads)
        t.join();
    auto t1 = __builtin_ia32_rdtsc();

    return t1 - t0;
}

template<class Queue>
void run_throughput_benchmark(char const* name) {
    int constexpr N = 1000000;
    int constexpr RUNS = 10;
    int constexpr PRODUCERS = 2;
    int constexpr CONSUMERS = 2;

    uint64_t min_time = std::numeric_limits<uint64_t>::max();
    for(unsigned run = RUNS; run--;) {
        uint64_t time = benchmark_throughput<Queue>(N, PRODUCERS, CONSUMERS);
        min_time = std::min(min_time, time);
    }

    double min_seconds = to_seconds(min_time);
    unsigned msg_per_sec = N * PRODUCERS / min_seconds;
    std::printf("%28s: %'11u msg/sec\n", name, msg_per_sec);
}

void run_throughput_benchmarks() {
    std::printf("---- Running throughput benchmarks (higher is better) ----\n");

    int constexpr CAPACITY = 65536;

    run_throughput_benchmark<BoostAdapter<boost::lockfree::queue<unsigned, boost::lockfree::capacity<CAPACITY - 2>>>>("boost::lockfree::queue");
    run_throughput_benchmark<RetryDecorator<AtomicQueueSpinlock<unsigned, CAPACITY>>>("pthread_spinlock");

    run_throughput_benchmark<RetryDecorator<AtomicQueue<unsigned, CAPACITY>>>("AtomicQueue");
    run_throughput_benchmark<AtomicQueue<unsigned, CAPACITY>>("BlockingAtomicQueue");
    run_throughput_benchmark<RetryDecorator<AtomicQueue2<unsigned, CAPACITY>>>("AtomicQueue2");
    run_throughput_benchmark<AtomicQueue2<unsigned, CAPACITY>>("BlockingAtomicQueue2");

    // run_throughput_benchmark<RetryDecorator<AtomicQueueSpinlockHle<unsigned, CAPACITY>>>("SpinlockHle");

    std::printf("\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue, bool Sender>
uint64_t ping_pong_thread(Barrier* barrier, Queue* q1, Queue* q2, unsigned N) {
    barrier->wait();

    uint64_t t0 = __builtin_ia32_rdtsc();
    for(unsigned i = 1, j = 0; j < N; ++i) {
        if(Sender) {
            q1->push(i);
            j = q2->pop();
        }
        else {
            j = q1->pop();
            q2->push(i);
        }
    }
    uint64_t t1 = __builtin_ia32_rdtsc();

    return t1 - t0;
}

template<class Queue>
inline std::array<uint64_t, 2> ping_pong_benchmark(unsigned N) {
    Queue q1, q2;
    Barrier barrier;
    auto time1 = std::async(std::launch::async, ping_pong_thread<Queue, false>, &barrier, &q1, &q2, N);
    auto time2 = std::async(std::launch::async, ping_pong_thread<Queue,  true>, &barrier, &q1, &q2, N);
    barrier.release(2);
    return {time1.get(), time2.get()};
}

template<class Queue>
void run_ping_pong_benchmark(char const* name) {
    int constexpr N = 100000;
    int constexpr RUNS = 10;

    // Select the best of RUNS runs.
    std::array<uint64_t, 2> best_times = {std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::max()};
    for(unsigned run = RUNS; run--;) {
        auto times = ping_pong_benchmark<Queue>(N);
        if(best_times[0] + best_times[1] > times[0] + times[1])
            best_times = times;
    }

    auto avg_time = (best_times[0] + best_times[1]) / (2 * 1e9 * CPU_FREQ);
    auto round_trip_time = avg_time / N;
    std::printf("%28s: %.9f sec/round-trip\n", name, round_trip_time);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void run_ping_pong_benchmarks() {
    std::printf("---- Running ping-pong benchmarks (lower is better) ----\n");

    // This benchmarks doesn't require queue capacity greater than 1, however, capacity of 1 elides
    // some instructions completely because of (x % 1) is always 0. Use something greater than 1 to
    // preclude aggressive optimizations.
    constexpr unsigned CAPACITY = 8;

    run_ping_pong_benchmark<BoostAdapter<boost::lockfree::spsc_queue<unsigned, boost::lockfree::capacity<CAPACITY>>>>("boost::lockfree::spsc_queue");
    run_ping_pong_benchmark<BoostAdapter<boost::lockfree::queue<unsigned, boost::lockfree::capacity<CAPACITY>>>>("boost::lockfree::queue");
    run_ping_pong_benchmark<RetryDecorator<AtomicQueueSpinlock<unsigned, CAPACITY>>>("pthread_spinlock");

    run_ping_pong_benchmark<RetryDecorator<AtomicQueue <unsigned, CAPACITY>>>("AtomicQueue");
    run_ping_pong_benchmark<AtomicQueue <unsigned, CAPACITY>>("BlockingAtomicQueue");
    run_ping_pong_benchmark<RetryDecorator<AtomicQueue2<unsigned, CAPACITY>>>("AtomicQueue2");
    run_ping_pong_benchmark<AtomicQueue2<unsigned, CAPACITY>>("BlockingAtomicQueue2");

    // run_ping_pong_benchmark<RetryDecorator<AtomicQueueSpinlockHle<unsigned, CAPACITY>>>("SpinlockHle");

    std::printf("\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main() {
    std::setlocale(LC_NUMERIC, "");

    run_throughput_benchmarks();
    run_ping_pong_benchmarks();

    static_cast<void>(run_ping_pong_benchmarks);
    static_cast<void>(run_throughput_benchmarks);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
