/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#include "atomic_queue_spin_lock.h"
#include "cpu_base_frequency.h"
#include "atomic_queue.h"
#include "barrier.h"

#include <boost/lockfree/spsc_queue.hpp>

#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <limits>
#include <vector>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using std::uint64_t;

using namespace ::atomic_queue;

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double const CPU_FREQ = ::atomic_queue::cpu_base_frequency();
// double const CPU_FREQ_INV = 1 / CPU_FREQ;
// uint64_t const NS_200 = 200 * CPU_FREQ;
// uint64_t const NS_1000 = 1000 * CPU_FREQ;
// double const NS_10_INV = 1 / (10 * CPU_FREQ);
// double const NS_100_INV = 1 / (100 * CPU_FREQ);

template<class T>
inline double to_seconds(T tsc) {
    return tsc / CPU_FREQ * 1e-9;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

auto constexpr STOP = static_cast<uint64_t>(1);

template<class Queue>
struct Stopper {
    std::atomic<unsigned> producer_count_;
    unsigned const consumer_count_;

    void operator()(Queue* const queue) {
        if(1 == producer_count_.fetch_sub(1, std::memory_order_relaxed)) {
            for(unsigned i = consumer_count_; i--;)
                queue->push(STOP);
        }
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// struct Bar {
//     unsigned width;

//     Bar(double width) : width(width + .5) {}

//     friend std::ostream& operator<<(std::ostream& s, Bar bar) {
//         while(bar.width--)
//             s.put('#');
//         return s;
//     }
// };

// struct Histogram {
//     unsigned bins_200[20] = {};
//     unsigned bins_1000[8] = {};
//     unsigned over_1000 = 0;

//     void update(uint64_t sample) {
//         if(sample < NS_200)
//             ++bins_200[static_cast<unsigned>(sample * NS_10_INV + .5)];
//         else if(sample < NS_1000)
//             ++bins_1000[static_cast<unsigned>(sample * NS_100_INV + .5) - 2];
//         else
//             ++over_1000;
//     }

//     friend std::ostream& operator<<(std::ostream& s, Histogram const& histogram) {
//         unsigned max = histogram.over_1000;
//         for(auto a : histogram.bins_200)
//             max = std::max(max, a);
//         for(auto a : histogram.bins_1000)
//             max = std::max(max, a);
//         double const bar_height = 100. / max;

//         for(unsigned i = 0; i < 20; ++i)
//             s << setw(5) << ((i + 1) * 10) << ' ' << setw(8) << histogram.bins_200[i] << ' ' << Bar{bar_height * histogram.bins_200[i]} << '\n';
//         for(unsigned i = 0; i < 8; ++i)
//             s << setw(5) << ((i + 3) * 100) << ' ' << setw(8) << histogram.bins_1000[i] << ' ' << Bar{bar_height * histogram.bins_1000[i]} << '\n';
//         s << "1000+ " << setw(8) << histogram.over_1000 << ' ' << Bar{bar_height * histogram.over_1000} << '\n';

//         return s;
//     }
// };

struct Stats {
    // alignas(64) Histogram histogram;
    uint64_t min = std::numeric_limits<uint64_t>::max();
    uint64_t max = std::numeric_limits<uint64_t>::min();
    uint64_t total_time = 0;
    double average = 0;

    void update(uint64_t sample) {
        // histogram.update(sample);
        min = std::min(sample, min);
        max = std::max(sample, max);
    }

    void merge(Stats const& b) {
        min = std::min(min, b.min);
        max = std::max(max, b.max);
        total_time = (total_time + b.total_time) / 2;
        average = (average + b.average) / 2;
    }

    // friend std::ostream& operator<<(std::ostream& s, Stats const& stats) {
    //     s << "Total time: " << std::fixed << std::setprecision(9) << stats.total_time / CPU_FREQ * 1e-9 << "ns. "
    //       << "Latencies min/avg/max: "
    //       << static_cast<unsigned>(stats.min / CPU_FREQ)
    //       << '/' << static_cast<unsigned>(stats.average / CPU_FREQ)
    //       << '/' << static_cast<unsigned>(stats.max / CPU_FREQ) << "ns\n"
    //       // << stats.histogram
    //         ;
    //     return s;
    // }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
Stats producer(Stopper<Queue>* stop, unsigned N, Queue* queue, ::atomic_queue::Barrier* barrier) {
    Stats stats;

    barrier->wait();

    uint64_t start = __builtin_ia32_rdtsc();
    uint64_t now = start;
    for(unsigned n = N; n--;) {
        queue->push(now);
        auto now2 = __builtin_ia32_rdtsc();
        stats.update(now2 - now);
        now = now2;
    }
    uint64_t end = __builtin_ia32_rdtsc();

    (*stop)(queue);

    stats.total_time = end - start;
    stats.average = static_cast<double>(stats.total_time) / N;

    return stats;
}

template<class Queue>
Stats consumer(Queue* queue, ::atomic_queue::Barrier* barrier) {
    Stats stats;

    barrier->wait();

    uint64_t latency_sum = 0;
    uint64_t start = __builtin_ia32_rdtsc();
    unsigned n = 0;
    for(;; ++n) {
        uint64_t producer_now = queue->pop();
        uint64_t now = __builtin_ia32_rdtsc();
        if(producer_now == STOP)
            break;
        auto latency = now - producer_now;
        latency_sum += latency;
        stats.update(latency);
    }
    uint64_t end = __builtin_ia32_rdtsc();

    stats.total_time = end - start;
    stats.average = static_cast<double>(latency_sum) / n;

    return stats;
}

template<class Queue>
std::array<Stats, 2> benchmark_latency(unsigned N, unsigned producer_count, unsigned consumer_count) {
    Queue queue;
    Stopper<Queue> stop{{producer_count}, consumer_count};

    Barrier barrier;
    std::vector<std::future<Stats>> producer_stats(producer_count), consumer_stats(consumer_count);
    for(unsigned i = 0; i < producer_count; ++i)
        producer_stats[i] = std::async(std::launch::async, producer<Queue>, &stop, N / producer_count, &queue, &barrier);
    for(unsigned i = 0; i < consumer_count; ++i)
        consumer_stats[i] = std::async(std::launch::async, consumer<Queue>, &queue, &barrier);
    barrier.release(producer_count + consumer_count);

    std::array<Stats, 2> producer_consumer_stats;
    for(unsigned i = 0; i < producer_count; ++i)
        producer_consumer_stats[0].merge(producer_stats[i].get());
    for(unsigned i = 0; i < consumer_count; ++i)
        producer_consumer_stats[1].merge(consumer_stats[i].get());

    return producer_consumer_stats;
}

template<class Queue>
void run_latency_benchmark(char const* name) {
    int constexpr N = 1000000;
    int constexpr RUNS = 10;

    std::array<Stats, 2> best_stats;
    best_stats[0].total_time = std::numeric_limits<decltype(best_stats[0].total_time)>::max();
    for(unsigned run = RUNS; run--;) {
        auto stats = benchmark_latency<Queue>(N, 2, 2);
        if(best_stats[0].total_time + best_stats[1].total_time > stats[0].total_time + stats[1].total_time)
            best_stats = stats;
    }

    std::printf(
        "%20s: "
        "Producers | Consumers: Time: %.9f | %.9f. Latency min/avg/max: %.9f/%.9f/%.9f | %.9f/%.9f/%.9f.\n",
        name,
        to_seconds(best_stats[0].total_time), to_seconds(best_stats[1].total_time),
        to_seconds(best_stats[0].min), to_seconds(best_stats[0].average), to_seconds(best_stats[0].max),
        to_seconds(best_stats[1].min), to_seconds(best_stats[1].average), to_seconds(best_stats[1].max)
        );
}

void run_latency_benchmarks() {
    std::printf("Running latency and throughput benchmarks...\n");
    int constexpr CAPACITY = 65536;
    run_latency_benchmark<RetryDecorator<AtomicQueue<uint64_t, CAPACITY>>>("AtomicQueue");
    run_latency_benchmark<AtomicQueue<uint64_t, CAPACITY>>("BlockingAtomicQueue");
    run_latency_benchmark<RetryDecorator<AtomicQueue2<uint64_t, CAPACITY>>>("AtomicQueue2");
    run_latency_benchmark<AtomicQueue2<uint64_t, CAPACITY>>("BlockingAtomicQueue2");
    run_latency_benchmark<RetryDecorator<AtomicQueueSpinlock<uint64_t, CAPACITY>>>("pthread_spinlock");
    run_latency_benchmark<RetryDecorator<AtomicQueueSpinlockHle<uint64_t, CAPACITY>>>("SpinlockHle");
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
    int constexpr N = 1000000;
    int constexpr RUNS = 10;

    // Select the best of RUNS runs.
    std::array<uint64_t, 2> best_times = {std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::max()};
    for(unsigned run = RUNS; run--;) {
        auto times = ping_pong_benchmark<Queue>(N);
        if(best_times[0] + best_times[1] > times[0] + times[1])
            best_times = times;
    }

    auto avg_time = (best_times[0] + best_times[1]) / (2 * 1e9 * CPU_FREQ);
    // auto time_diff = static_cast<int64_t>(best_times[0] - best_times[1]) / (1e9 * CPU_FREQ);
    auto round_trip_time = avg_time / N;
    std::printf("%20s: Time: %.9f. Round trip time: %.9f.\n", name, avg_time, round_trip_time);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
struct SpscAdapter : Queue {
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

void run_ping_pong_benchmarks() {
    std::printf("Running ping-pong benchmarks...\n");

    // This benchmarks doesn't require queue capacity greater than 1, however, capacity of 1 elides
    // some instructions completely because of (x % 1) is always 0. Use something greater than 1 to
    // preclude aggressive optimizations.
    constexpr unsigned CAPACITY = 8;

    run_ping_pong_benchmark<SpscAdapter<boost::lockfree::spsc_queue<unsigned, boost::lockfree::capacity<CAPACITY>>>>("boost::spsc_queue");
    run_ping_pong_benchmark<RetryDecorator<AtomicQueue <unsigned, CAPACITY>>>("AtomicQueue");
    run_ping_pong_benchmark<AtomicQueue <unsigned, CAPACITY>>("BlockingAtomicQueue");
    run_ping_pong_benchmark<RetryDecorator<AtomicQueue2<unsigned, CAPACITY>>>("AtomicQueue2");
    run_ping_pong_benchmark<AtomicQueue2<unsigned, CAPACITY>>("BlockingAtomicQueue2");
    run_ping_pong_benchmark<RetryDecorator<AtomicQueueSpinlock<unsigned, CAPACITY>>>("pthread_spinlock");
    run_ping_pong_benchmark<RetryDecorator<AtomicQueueSpinlockHle<unsigned, CAPACITY>>>("SpinlockHle");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main() {
    // std::cout << "CPU base frequency is " << CPU_FREQ << "GHz.\n";

    run_latency_benchmarks();
    run_ping_pong_benchmarks();

    static_cast<void>(run_ping_pong_benchmarks);
    static_cast<void>(run_latency_benchmarks);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
