/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#include "atomic_queue_spin_lock.h"
#include "cpu_base_frequency.h"
#include "atomic_queue.h"
#include "barrier.h"

#include <boost/lockfree/spsc_queue.hpp>

#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <limits>
#include <vector>
#include <thread>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using std::uint64_t;
using std::setw;

using namespace ::atomic_queue;

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double const CPU_FREQ = ::atomic_queue::cpu_base_frequency();
double const CPU_FREQ_INV = 1 / CPU_FREQ;
uint64_t const NS_200 = 200 * CPU_FREQ;
uint64_t const NS_1000 = 1000 * CPU_FREQ;
double const NS_10_INV = 1 / (10 * CPU_FREQ);
double const NS_100_INV = 1 / (100 * CPU_FREQ);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void set_affinity(char const* env_name) {
//     char const* value = std::getenv(env_name);
//     if(!value)
//         return;
//     unsigned affinity = std::stoi(value);
//     if(affinity >= CPU_SETSIZE)
//         throw;

//     cpu_set_t cpuset;
//     CPU_ZERO(&cpuset);
//     CPU_SET(affinity, &cpuset);
//     if(int e = pthread_setaffinity_np(pthread_self(), sizeof cpuset, &cpuset))
//         throw std::system_error(e, std::system_category(), "pthread_setaffinity_np");
//     // std::printf("affinity %u set.\n", affinity);
// }

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
                    _mm_pause();
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
    // using Queue = ::atomic_queue::BlockingAtomicQueue<uint64_t, 100000>;
    using Queue = ::atomic_queue::AtomicQueue2<uint64_t, 100000>;
    // using Queue = ::atomic_queue::BlockingAtomicQueue2<uint64_t, 100000>;
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

template<class Queue, bool Sender>
uint64_t ping_pong_thread(Barrier* barrier, Queue* q1, Queue* q2, unsigned N) {
    // set_affinity(Sender ? "AQT1" : "AQT2");

    barrier->wait();

    uint64_t t0 = __builtin_ia32_rdtsc();
    for(unsigned i = 1, j = 0; j < N; ++i) {
        if constexpr(Sender) {
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
    alignas(CACHE_LINE_SIZE) Queue q1;
    alignas(CACHE_LINE_SIZE) Queue q2;
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
    auto time_diff = static_cast<int64_t>(best_times[0] - best_times[1]) / (1e9 * CPU_FREQ);
    auto round_trip_time = avg_time / N;
    std::printf("%20s: %.9f seconds. Round trip time: %.9f seconds. Difference: %.9f seconds.\n", name, avg_time, round_trip_time, time_diff);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, class Queue>
struct BlockingAdapter : Queue {
    using Queue::Queue;

    void push(T element) {
        while(!this->try_push(element))
            _mm_pause();
    }

    T pop() {
        T element;
        while(!this->try_pop(element))
            _mm_pause();
        return element;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, class Queue>
struct SpscAdapter : Queue {
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
    constexpr unsigned C = 4;
    run_ping_pong_benchmark<SpscAdapter<unsigned, boost::lockfree::spsc_queue<unsigned, boost::lockfree::capacity<C>>>>("boost::spsc_queue");
    run_ping_pong_benchmark<BlockingAdapter<unsigned, AtomicQueue <unsigned, C>>>("AtomicQueue");
    run_ping_pong_benchmark<BlockingAtomicQueue <unsigned, C>>("BlockingAtomicQueue");
    run_ping_pong_benchmark<BlockingAdapter<unsigned, AtomicQueue2<unsigned, C>>>("AtomicQueue2");
    run_ping_pong_benchmark<BlockingAtomicQueue2<unsigned, C>>("BlockingAtomicQueue2");
    run_ping_pong_benchmark<BlockingAdapter<unsigned, AtomicQueueSpinLock<unsigned, C>>>("pthread_spinlock_t");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main() {
    // std::cout << "CPU base frequency is " << CPU_FREQ << "GHz.\n";
    run_ping_pong_benchmarks();

    // int constexpr N = 1000000;
    // benchmark_latency(N, 2, 2);
    static_cast<void>(benchmark_latency);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
