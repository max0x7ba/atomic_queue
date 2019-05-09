/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#include "cpu_base_frequency.h"
#include "atomic_queue.h"
#include "atomic_queue_spin_lock.h"
#include "barrier.h"

#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <limits>
#include <vector>
#include <thread>

#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using std::uint64_t;
using std::setw;

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double const CPU_FREQ = ::atomic_queue::cpu_base_frequency();
double const CPU_FREQ_INV = 1 / CPU_FREQ;
uint64_t const NS_200 = 200 * CPU_FREQ;
uint64_t const NS_1000 = 1000 * CPU_FREQ;
double const NS_10_INV = 1 / (10 * CPU_FREQ);
double const NS_100_INV = 1 / (100 * CPU_FREQ);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

auto constexpr STOP = static_cast<uint64_t>(-1);

template<class Queue>
struct Stopper {
    std::atomic<unsigned> producer_count_;
    unsigned const consumer_count_;

    void operator()(Queue* const queue) {
        if(1 == producer_count_.fetch_sub(1, std::memory_order_relaxed)) {
            for(unsigned i = consumer_count_; i--;)
                while(!queue->try_push(STOP))
                    boost::atomics::detail::pause();
        }
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Bar {
    unsigned width;

    Bar(double width) : width(width + .5) {}

    friend std::ostream& operator<<(std::ostream& s, Bar bar) {
        while(bar.width--)
            s.put('#');
        return s;
    }
};

struct Histogram {
    unsigned bins_200[20] = {};
    unsigned bins_1000[8] = {};
    unsigned over_1000 = 0;

    void update(uint64_t sample) {
        if(sample < NS_200)
            ++bins_200[static_cast<unsigned>(sample * NS_10_INV + .5)];
        else if(sample < NS_1000)
            ++bins_1000[static_cast<unsigned>(sample * NS_100_INV + .5) - 2];
        else
            ++over_1000;
    }

    friend std::ostream& operator<<(std::ostream& s, Histogram const& histogram) {
        unsigned max = histogram.over_1000;
        for(auto a : histogram.bins_200)
            max = std::max(max, a);
        for(auto a : histogram.bins_1000)
            max = std::max(max, a);
        double const bar_height = 100. / max;

        for(unsigned i = 0; i < 20; ++i)
            s << setw(5) << ((i + 1) * 10) << ' ' << setw(8) << histogram.bins_200[i] << ' ' << Bar{bar_height * histogram.bins_200[i]} << '\n';
        for(unsigned i = 0; i < 8; ++i)
            s << setw(5) << ((i + 3) * 100) << ' ' << setw(8) << histogram.bins_1000[i] << ' ' << Bar{bar_height * histogram.bins_1000[i]} << '\n';
        s << "1000+ " << setw(8) << histogram.over_1000 << ' ' << Bar{bar_height * histogram.over_1000} << '\n';

        return s;
    }
};

struct Stats {
    alignas(64) Histogram histogram;
    uint64_t min = std::numeric_limits<uint64_t>::max();
    uint64_t max = std::numeric_limits<uint64_t>::min();
    uint64_t total_time = 0;
    double average = std::numeric_limits<double>::quiet_NaN();

    void update(uint64_t sample) {
        histogram.update(sample);
        min = std::min(sample, min);
        max = std::max(sample, max);
    }

    friend std::ostream& operator<<(std::ostream& s, Stats const& stats) {
        s << "total time: " << std::fixed << std::setprecision(9) << stats.total_time / CPU_FREQ * 1e-9 << "ns\n"
          << "latencies min/avg/max: "
          << static_cast<unsigned>(stats.min / CPU_FREQ)
          << '/' << static_cast<unsigned>(stats.average / CPU_FREQ)
          << '/' << static_cast<unsigned>(stats.max / CPU_FREQ) << "ns\n"
          << stats.histogram;
        return s;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
void producer(Stopper<Queue>* stop, unsigned N, Queue* queue, ::atomic_queue::Barrier* barrier, Stats* stats_result) {
    barrier->wait();

    Stats stats;

    uint64_t start = __builtin_ia32_rdtsc();
    uint64_t now = __builtin_ia32_rdtsc();
    for(unsigned n = N; n--;) {
        while(!queue->try_push(now))
            ;
        auto now2 = __builtin_ia32_rdtsc();
        stats.update(now2 - now);
        now = now2;
    }
    uint64_t end = __builtin_ia32_rdtsc();

    (*stop)(queue);

    stats.total_time = end - start;
    *stats_result = stats;
}

template<class Queue>
void consumer(Queue* queue, ::atomic_queue::Barrier* barrier, Stats* stats_result) {
    barrier->wait();

    Stats stats;
    uint64_t latency_sum = 0;

    uint64_t start = __builtin_ia32_rdtsc();
    unsigned n = 0;
    for(;; ++n) {
        uint64_t producer_now;
        while(!queue->try_pop(producer_now))
            ;
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

    *stats_result = stats;
}

void benchmark_latency(unsigned N, unsigned producer_count, unsigned consumer_count) {
    // using Queue = ::atomic_queue::AtomicQueue<uint64_t, 100000>;
    using Queue = ::atomic_queue::AtomicQueue2<uint64_t, 100000>;
    // using Queue = ::atomic_queue::BlockingAtomicQueue3<uint64_t, 100000>;
    // using Queue = ::atomic_queue::AtomicQueueSpinLock<uint64_t, 100000>;
    Queue queue;
    ::atomic_queue::Barrier barrier;
    Stopper<Queue> stop{{producer_count}, consumer_count};

    std::vector<Stats> producer_stats(producer_count), consumer_stats(consumer_count);
    std::vector<std::thread> threads(producer_count + consumer_count);
    for(unsigned i = 0; i < producer_count; ++i)
        threads[i] = std::thread(producer<Queue>, &stop, N / producer_count, &queue, &barrier, &producer_stats[i]);
    for(unsigned i = 0; i < consumer_count; ++i)
        threads[producer_count + i] = std::thread(consumer<Queue>, &queue, &barrier, &consumer_stats[i]);

    barrier.release(producer_count + consumer_count);

    for(auto& thread : threads)
        thread.join();

    std::cout << "Producer stats:\n";
    for(auto const& stats : producer_stats)
        std::cout << stats;
    std::cout << "Consumer stats:\n";
    for(auto const& stats : consumer_stats)
        std::cout << stats;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main() {
    std::cout << "pid: " << getpid() << '\n';
    std::cout << "CPU base frequency is " << CPU_FREQ << "GHz\n";

    int constexpr N = 1000000;
    benchmark_latency(N, 2, 2);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
