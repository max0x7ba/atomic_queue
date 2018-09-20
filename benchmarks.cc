/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#include "atomic_queue.h"
#include "barrier.h"

#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <limits>
#include <vector>
#include <thread>
#include <iostream>
#include <regex>
#include <fstream>

#include <boost/dynamic_bitset.hpp>

#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using std::uint64_t;

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double CPU_base_frequency() {
    std::regex re("model name\\s*:[^@]+@\\s*([0-9.]+)\\s*GHz");
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::smatch m;
    for(std::string line; getline(cpuinfo, line);) {
        regex_match(line, m, re);
        if(m.size() == 2)
            return std::stod(line.substr(m[1].first - line.begin(), m[1].second - m[1].first));
    }
    return 1e9;
}

double const CPU_FREQ = CPU_base_frequency();

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

struct Stats {
    uint64_t total_time = 0;
    double average = std::numeric_limits<double>::quiet_NaN();
    uint64_t min = std::numeric_limits<uint64_t>::max();
    uint64_t max = std::numeric_limits<uint64_t>::min();

    void update(uint64_t a) {
        min = std::min(a, min);
        max = std::max(a, max);
    }

    friend std::ostream& operator<<(std::ostream& s, Stats const& stats) {
        return s
            << "total time: " << uint64_t(stats.total_time / CPU_FREQ) << "ns "
            << "latencies min/avg/max: " << stats.min / CPU_FREQ << '/' << stats.average / CPU_FREQ << '/' << stats.max / CPU_FREQ << "ns\n";
            ;
    }
};

template<class Queue>
void producer(Stopper<Queue>* stop, unsigned N, Queue* queue, ::atomic_queue::Barrier* barrier, Stats* stats_result) {
    barrier->wait();

    Stats stats;

    uint64_t start = __builtin_ia32_rdtsc();
    for(unsigned n = N; n--;) {
        uint64_t now = __builtin_ia32_rdtsc();
        if(!queue->try_push(now))
            throw std::runtime_error("Queue overflow.");
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
    using Queue = ::atomic_queue::AtomicQueue<uint64_t, 100000>;
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

// void fetch_add_test_thread(unsigned n, ::atomic_queue::Barrier* barrier, std::atomic<unsigned>* a, boost::dynamic_bitset<>* bitset) {
//     barrier->wait();
//     while(n--) {
//         auto i = a->fetch_add(1, std::memory_order_acq_rel);
//         bitset->set(i);
//     }
// }

// void fetch_add_test() {
//     unsigned constexpr N = 1000000;
//     unsigned constexpr THREADS = 4;
//     ::atomic_queue::Barrier barrier;
//     std::atomic<unsigned> a{0};
//     boost::dynamic_bitset<> bitsets[THREADS];
//     std::vector<std::thread> threads;
//     threads.reserve(THREADS);
//     for(auto& b : bitsets) {
//         b.resize(N * THREADS);
//         threads.emplace_back(fetch_add_test_thread, N, &barrier, &a, &b);
//     }
//     barrier.release(THREADS);
//     for(auto& thread : threads)
//         thread.join();

//     boost::dynamic_bitset<> u(N * THREADS);
//     u.set();
//     for(auto& b : bitsets)
//         u &= b;
//     std::cout << "fetch_add_test: " << u.any() << '\n';
// }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main() {
    std::cout << "pid: " << getpid() << '\n';
    std::cout << "CPU base frequency is " << CPU_FREQ << "GHz\n";

    int constexpr N = 1000000;
    benchmark_latency(N, 1, 2);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
