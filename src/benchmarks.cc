/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "atomic_queue/atomic_queue.h"
#include "atomic_queue/atomic_queue_mutex.h"
#include "atomic_queue/barrier.h"

#include <xenium/michael_scott_queue.hpp>
#include <xenium/ramalhete_queue.hpp>
#include <xenium/vyukov_bounded_queue.hpp>
#include <xenium/reclamation/generic_epoch_based.hpp>

#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include <tbb/concurrent_queue.h>
#include <tbb/spin_mutex.h>

#include "cpu_base_frequency.h"
#include "huge_pages.h"
#include "moodycamel.h"
#include "benchmarks.h"


#include <algorithm>
#include <clocale>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using std::uint64_t;
using std::int64_t;

using std::printf;
using std::fprintf;

using namespace ::atomic_queue;
namespace A = ::atomic_queue;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int constexpr N_MSG = 1'000'000;
int constexpr RUNS = 3;

struct Options : EnvBits64 {
    ATOMIC_QUEUE_INLINE constexpr auto       minimal() const noexcept { return value & 1; };

    ATOMIC_QUEUE_INLINE constexpr auto  no_ping_pong() const noexcept { return value & 2; };
    ATOMIC_QUEUE_INLINE constexpr auto no_throughput() const noexcept { return value & 4; };

    ATOMIC_QUEUE_INLINE constexpr auto  no_variant_a() const noexcept { return value & 8; };
    ATOMIC_QUEUE_INLINE constexpr auto  no_variant_b() const noexcept { return value & 16; };
    ATOMIC_QUEUE_INLINE constexpr auto  no_variant_1() const noexcept { return value & 32; };
    ATOMIC_QUEUE_INLINE constexpr auto  no_variant_2() const noexcept { return value & 64; };

    ATOMIC_QUEUE_INLINE constexpr auto       no_spsc() const noexcept { return value & 128; };
};

struct Params {
    Options options{"AQB"};
    int n_msg = EnvBits64{"AQN", N_MSG, 1, INT_MAX}.value;
    std::vector<unsigned> hw_thread_ids;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T>
using Type = std::common_type<T>; // Similar to boost::type<>.

using sum_t = long long;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class P>
struct Range {
    P p, q;
    auto begin() const noexcept { return p; }
    auto end() const noexcept { return q; }
};

template<class P> Range<P> as_range(P p, P q) noexcept { return {p, q}; }
template<class P> Range<P> as_range(P p, size_t n) noexcept { return {p, p + n}; }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using cycles_t = decltype(__rdtsc());
static_assert(std::is_unsigned<cycles_t>::value);

using icycles_t = std::make_signed<cycles_t>::type; // Signed integers convert into double with one AVX instruction, unlike unsigned.
cycles_t constexpr CYCLES_MAX = -1;

ATOMIC_QUEUE_INLINE static cycles_t cycles() noexcept {
    // If software requires RDTSC to be executed only after all previous instructions have executed and all previous loads are
    // globally visible, it can execute LFENCE immediately before RDTSC.
    _mm_lfence();
    return __rdtsc();
}

double TSC_TO_SECONDS = 0; // Set in main.

ATOMIC_QUEUE_INLINE double to_seconds(icycles_t cycles) noexcept {
    return cycles * TSC_TO_SECONDS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
struct BoostSpScAdapter : Queue {
    using T = typename Queue::value_type;

    ATOMIC_QUEUE_INLINE void push(T element) {
        while(!this->Queue::push(element))
            spin_loop_pause();
    }

    ATOMIC_QUEUE_INLINE T pop() {
        T element;
        while(!this->Queue::pop(element))
            spin_loop_pause();
        return element;
    }
};

template<class Queue>
struct BoostQueueAdapter : BoostSpScAdapter<Queue> {
    using T = typename Queue::value_type;

    ATOMIC_QUEUE_INLINE void push(T element) {
        while(!this->Queue::bounded_push(element))
            spin_loop_pause();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using Reclaimer = xenium::reclamation::new_epoch_based<>;

template<class Queue>
struct XeniumQueueAdapter : Queue {
    using T = typename Queue::value_type;

    ATOMIC_QUEUE_INLINE T pop() {
        T element;
        while(!this->Queue::try_pop(element))
            spin_loop_pause();
        return element;
    }
};

template <class T>
struct region_guard_traits{
    struct region_guard { constexpr region_guard() noexcept = default; };
};
template <class T, class... Policies>
struct region_guard_traits<xenium::michael_scott_queue<T, Policies...>> {
    using region_guard = typename xenium::michael_scott_queue<T, Policies...>::region_guard;
};
template <class T, class... Policies>
struct region_guard_traits<xenium::ramalhete_queue<T, Policies...>> {
    using region_guard = typename xenium::ramalhete_queue<T, Policies...>::region_guard;
};

template <class T>
using region_guard_t = typename region_guard_traits<T>::region_guard;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue, size_t Capacity>
struct TbbAdapter : RetryDecorator<Queue> {
    ATOMIC_QUEUE_INLINE TbbAdapter() {
        this->set_capacity(Capacity);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using Allocator = HugePageAllocator<unsigned>;
using BoostAllocator = boost::lockfree::allocator<Allocator>;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// According to my benchmarking, it looks like the best performance is achieved with the following parameters:
// * For SPSC: SPSC=true,  MINIMIZE_CONTENTION=false, MAXIMIZE_THROUGHPUT=false.
// * For MPMC: SPSC=false, MINIMIZE_CONTENTION=true,  MAXIMIZE_THROUGHPUT=true.
// However, I am not sure that conflating these 3 parameters into 1 would be the right thing for every scenario.
template<unsigned SIZE, bool SPSC, bool MINIMIZE_CONTENTION, bool MAXIMIZE_THROUGHPUT>
struct QueueTypes {
    using T = unsigned;

    // For atomic elements only.
    using AtomicQueue =                            RetryDecorator<A::AtomicQueue<T, SIZE, T{}, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, false, SPSC>>;
    using OptimistAtomicQueue =                                   A::AtomicQueue<T, SIZE, T{}, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, false, SPSC>;
    using AtomicQueueB =        RetryDecorator<CapacityArgAdaptor<A::AtomicQueueB<T, Allocator, T{}, MAXIMIZE_THROUGHPUT, false, SPSC>, SIZE>>;
    using OptimistAtomicQueueB =               CapacityArgAdaptor<A::AtomicQueueB<T, Allocator, T{}, MAXIMIZE_THROUGHPUT, false, SPSC>, SIZE>;

    // For non-atomic elements.
    using AtomicQueue2 =                     RetryDecorator<A::AtomicQueue2<T, SIZE, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, false, SPSC>>;
    using OptimistAtomicQueue2 =                            A::AtomicQueue2<T, SIZE, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, false, SPSC>;
    using AtomicQueueB2 = RetryDecorator<CapacityArgAdaptor<A::AtomicQueueB2<T, Allocator, MAXIMIZE_THROUGHPUT, false, SPSC>, SIZE>>;
    using OptimistAtomicQueueB2 =        CapacityArgAdaptor<A::AtomicQueueB2<T, Allocator, MAXIMIZE_THROUGHPUT, false, SPSC>, SIZE>;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Times {
    std::atomic<cycles_t> t[2] = {};

    ATOMIC_QUEUE_INLINE void set(unsigned i) noexcept {
        auto* p = t + i; // Resolve the address into a register before cycles.
        auto now = cycles();
        *p = now; // std::memory_order_seq_cst
    }

    ATOMIC_QUEUE_INLINE cycles_t get(unsigned i) const noexcept {
        return t[i].load(X);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ThreadState {
    alignas(CACHE_LINE_SIZE)
    Times times;
    std::atomic<sum_t> sum = {};

    std::thread thread;
};
using ThreadStates = std::vector<ThreadState, HugePageAllocator<ThreadState>>;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SharedState {
    // These remain constant.
    alignas(CACHE_LINE_SIZE)
    unsigned const n_msg;
    unsigned const n_threads;

    void* queue0 = 0;
    void* queue1 = 0;

    ThreadState* const threads;
    unsigned const* ATOMIC_QUEUE_RESTRICT hw_thread_ids;

    unsigned thread_idx = 0;

    // These are modified at the start.
    alignas(CACHE_LINE_SIZE)
    Barrier2 barrier;

    ATOMIC_QUEUE_INLINE SharedState(Params const* params, unsigned thread_count, ThreadState* consumer_sums) noexcept
        : n_msg(params->n_msg)
        , n_threads(thread_count)
        , threads(consumer_sums)
        , hw_thread_ids{params->hw_thread_ids.data()}
        , barrier{thread_count * 2}
    {
        assert(is_suitably_aligned(this));
    }

    ATOMIC_QUEUE_INLINE auto as_thread_range() noexcept {
        return as_range(threads, n_threads * 2);
    }

    ATOMIC_QUEUE_INLINE auto* reuse_this_thread() noexcept {
        set_thread_affinity(hw_thread_ids[thread_idx]); // Use this thread#0 for the first producer. Pin to the same CPU.
        return threads + thread_idx++;
    }

    template<class... Args>
    ATOMIC_QUEUE_NOINLINE void create_thread(Args... args) {
        set_default_thread_affinity(hw_thread_ids[thread_idx]);
        auto& thr = threads[thread_idx];
        thr.thread = std::thread(args..., this, &thr);
        ++thread_idx;
    }

    ATOMIC_QUEUE_NOINLINE void join() {
        for(auto& thr : as_thread_range())
            if(thr.thread.joinable())
                thr.thread.join();
    }

    ATOMIC_QUEUE_NOINLINE cycles_t total_time() const noexcept {
        cycles_t t0 = CYCLES_MAX, t1 = 0;
        for(auto& thr : as_range(threads, n_threads * 2)) {
            t0 = min_value(t0, thr.times.get(0));
            t1 = max_value(t1, thr.times.get(1));
        }

        if(ATOMIC_QUEUE_UNLIKELY(t0 == CYCLES_MAX || !t1)) // Must never happen.
            std::abort();

        return t1 - t0;
    }
};

struct SharedState2 : SharedState {
    ThreadState threads2[2];

    template<class... Args>
    constexpr ATOMIC_QUEUE_INLINE SharedState2(Params const* params, unsigned const (&cpus)[2])
        : SharedState{params, 1u, threads2}
    {
        this->hw_thread_ids = cpus;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
ATOMIC_QUEUE_NOINLINE void throughput_producer(SharedState* ctx, ThreadState* thread) {
    Queue* const queue = static_cast<Queue*>(ctx->queue0);

    [[maybe_unused]] region_guard_t<Queue> guard;
    ProducerOf<Queue> producer{*queue};
    unsigned n = ctx->n_msg;

    ctx->barrier.countdown();
    thread->times.set(0);

    do
        producer.push(*queue, n);
    while(--n);

    thread->times.set(1);
}

template<class Queue>
ATOMIC_QUEUE_NOINLINE void throughput_consumer(SharedState* ctx, ThreadState* thread) {
    Queue* const queue = static_cast<Queue*>(ctx->queue0);

    [[maybe_unused]] region_guard_t<Queue> guard;
    ConsumerOf<Queue> consumer{*queue};
    sum_t sum = 1; // Set sums are +1 biased.
    unsigned n;

    ctx->barrier.countdown();
    thread->times.set(0);

    do {
        n = consumer.pop(*queue);
        sum += n; // Includes stop value.
    } while(n > 1);

    thread->sum = sum; // memory_order_seq_cst
    thread->times.set(1);
}

template<class Queue>
ATOMIC_QUEUE_INLINE cycles_t time_throughput_once(Params const* params, unsigned thread_count, bool alternative_placement, ThreadState* consumer_sums) {
    auto ctx = HugePages::instance->create_unique_ptr<SharedState>(params, thread_count, consumer_sums);
    auto queue = HugePages::instance->create_unique_ptr<Queue>(ContextOf<Queue>{thread_count, thread_count});
    ctx->queue0 = queue.get();

    auto* producer0 = ctx->reuse_this_thread(); // Use this thread#0 for the first producer.

    if(alternative_placement) {
        for(unsigned i = 0; i < thread_count; ++i) {
            if(i) // This thread#0 is the first producer.
                ctx->create_thread(throughput_producer<Queue>);
            ctx->create_thread(throughput_consumer<Queue>);
        }
    } else {
        for(unsigned i = 1; i < thread_count; ++i)  // This thread#0 is the first producer.
            ctx->create_thread(throughput_producer<Queue>);
        for(unsigned i = 0; i < thread_count; ++i)
            ctx->create_thread(throughput_consumer<Queue>);
    }

    throughput_producer<Queue>(ctx.get(), producer0); // Use this thread#0 for the first producer.
    ctx->join();

    return ctx->total_time();
}

template<class Queue>
ATOMIC_QUEUE_NOINLINE void time_throughput(char const* name, Params const* params, unsigned n_thread_min, unsigned n_thread_max) {
    int const N = params->n_msg;
    sum_t const expected_sum = ((N + 1) * .5) * N;
    double const expected_sum_inv = 1. / expected_sum;

    for(unsigned n_threads = n_thread_min; n_threads <= n_thread_max; ++n_threads) {
        for(bool alternative_placement : {false, true}) {
            cycles_t min_time = CYCLES_MAX;

            for(unsigned run = RUNS; run--;) {
                ThreadStates threads(n_threads * 2);
                cycles_t time = time_throughput_once<Queue>(params, n_threads, alternative_placement, threads.data());
                min_time = min_value(min_time, time);

                // Calculate the checksum.
                sum_t total_sum = 0;
                unsigned i = 0;
                for(auto& thr : threads) {
                    auto consumer_sum = thr.sum.load(X);
                    // Set sums are +1 biased.
                    if(consumer_sum--) {
                        total_sum += consumer_sum;
                        // Verify that no consumer was starved.
                        auto consumer_sum_frac = consumer_sum * expected_sum_inv;
                        if(consumer_sum_frac < .1)
                            fprintf(stderr, "%s: producers: %u: consumer %u received too few messages: %.2lf%% of expected.\n",
                                    name, n_threads, i, consumer_sum_frac);
                        ++i;
                    }
                }

                threads = ThreadStates(); // Deallocate memory.
                HugePages::instance->check_huge_pages_leaks(name);

                // Verify that all messages were received exactly once: no duplicates, no omissions.
                if(auto diff = total_sum - expected_sum * n_threads)
                    fprintf(stderr, "%s: wrong checksum error: producers: %u, expected_sum: %'lld, diff: %'lld.\n",
                            name, n_threads, expected_sum * n_threads, diff);
            }

            double min_seconds = to_seconds(min_time);
            unsigned msg_per_sec = N * n_threads / min_seconds;
            printf("%32s,%2u,%c: %'11u msg/sec\n", name, n_threads, alternative_placement ? 'i' : 's', msg_per_sec);
        }
    }
}

template<class Queue>
ATOMIC_QUEUE_INLINE void time_throughput_mpmc(char const* name, Params const* params, Type<Queue>, unsigned thread_count_min = 1) {
    unsigned const thread_count_max = params->hw_thread_ids.size() / 2;
    time_throughput<Queue>(name, params, thread_count_min, thread_count_max);
}

template<class... Args>
ATOMIC_QUEUE_INLINE void time_throughput_spsc(char const* name, Params const* params,
                                   Type<BoostSpScAdapter<boost::lockfree::spsc_queue<Args...>>>) {
    using Queue = BoostSpScAdapter<boost::lockfree::spsc_queue<Args...>>;
    time_throughput<Queue>(name, params, 1, 1); // spsc_queue can only handle 1 producer and 1 consumer.
}

template<class Queue>
ATOMIC_QUEUE_INLINE void time_throughput_spsc(char const* name, Params const* params, Type<Queue>) {
    time_throughput<Queue>(name, params, 1, 1); // Special case for 1 producer and 1 consumer.
}

ATOMIC_QUEUE_NOINLINE void run_throughput_benchmarks(Params const* params) {
    printf("---- Running throughput benchmarks with up to %zu CPUs (higher is better) ----\n", params->hw_thread_ids.size() & -2);

    int constexpr SIZE = 65536;

    // The reference.
    if(ATOMIC_QUEUE_LIKELY(!params->options.no_spsc()))
        time_throughput_spsc("boost::lockfree::spsc_queue", params,
                             Type<BoostSpScAdapter<boost::lockfree::spsc_queue<unsigned, boost::lockfree::capacity<SIZE>>>>{});

    if(ATOMIC_QUEUE_LIKELY(!params->options.minimal())) {
        time_throughput_mpmc("boost::lockfree::queue", params,
                             Type<BoostQueueAdapter<boost::lockfree::queue<unsigned, BoostAllocator, boost::lockfree::capacity<SIZE - 2>>>>{});

        // time_throughput_mpmc("TicketSpinlock", params, Type<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, TicketSpinlock>>>{});
        // run_throughput_mpmc_benchmark("UnfairSpinlock", params, Type<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, UnfairSpinlock>>>{});
        // run_throughput_mpmc_benchmark<RetryDecorator<AtomicQueueSpinlockHle<unsigned, SIZE>>>("SpinlockHle");

        time_throughput_spsc("moodycamel::ReaderWriterQueue", params, Type<MoodyCamelReaderWriterQueue<unsigned, SIZE>>{});
        time_throughput_mpmc("moodycamel::ConcurrentQueue", params, Type<MoodyCamelQueue<unsigned, SIZE>>{});

        time_throughput_mpmc("pthread_spinlock", params, Type<RetryDecorator<AtomicQueueSpinlock<unsigned, SIZE>>>{});
        time_throughput_mpmc("std::mutex", params, Type<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, std::mutex>>>{});
        time_throughput_mpmc("tbb::spin_mutex", params, Type<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, tbb::spin_mutex>>>{});
        // run_throughput_mpmc_benchmark("adaptive_mutex", params, Type<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, AdaptiveMutex>>>{});
        // run_throughput_mpmc_benchmark("tbb::speculative_spin_mutex", params,
        //                               Type<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, tbb::speculative_spin_mutex>>>{});
        time_throughput_mpmc("tbb::concurrent_bounded_queue", params, Type<TbbAdapter<tbb::concurrent_bounded_queue<unsigned>, SIZE>>{});

        time_throughput_mpmc("xenium::michael_scott_queue", params,
            Type<XeniumQueueAdapter<xenium::michael_scott_queue<unsigned, xenium::policy::reclaimer<Reclaimer>>>>{});
        time_throughput_mpmc("xenium::ramalhete_queue", params,
            Type<XeniumQueueAdapter<xenium::ramalhete_queue<unsigned, xenium::policy::reclaimer<Reclaimer>>>>{});
        time_throughput_mpmc("xenium::vyukov_bounded_queue", params,
            Type<RetryDecorator<CapacityArgAdaptor<xenium::vyukov_bounded_queue<unsigned>, SIZE>>>{});
    }

    using SPSC = QueueTypes<SIZE, true, false, false>;
    using MPMC = QueueTypes<SIZE, false, true, true>; // Enable MAXIMIZE_THROUGHPUT for 2 or more producers/consumers.

    if(ATOMIC_QUEUE_LIKELY(!params->options.no_variant_1())) {
        if(ATOMIC_QUEUE_LIKELY(!params->options.no_variant_a())) {
            if(ATOMIC_QUEUE_LIKELY(!params->options.no_spsc()))
                time_throughput_spsc("AtomicQueue", params, Type<SPSC::AtomicQueue>{});
            time_throughput_mpmc("AtomicQueue", params, Type<MPMC::AtomicQueue>{}, 2);

            if(ATOMIC_QUEUE_LIKELY(!params->options.no_spsc()))
                time_throughput_spsc("OptimistAtomicQueue", params, Type<SPSC::OptimistAtomicQueue>{});
            time_throughput_mpmc("OptimistAtomicQueue", params, Type<MPMC::OptimistAtomicQueue>{}, 2);
        }

        if(ATOMIC_QUEUE_LIKELY(!params->options.no_variant_b())) {
            if(ATOMIC_QUEUE_LIKELY(!params->options.no_spsc()))
                time_throughput_spsc("AtomicQueueB", params, Type<SPSC::AtomicQueueB>{});
            time_throughput_mpmc("AtomicQueueB", params, Type<MPMC::AtomicQueueB>{}, 2);

            if(ATOMIC_QUEUE_LIKELY(!params->options.no_spsc()))
                time_throughput_spsc("OptimistAtomicQueueB", params, Type<SPSC::OptimistAtomicQueueB>{});
            time_throughput_mpmc("OptimistAtomicQueueB", params, Type<MPMC::OptimistAtomicQueueB>{}, 2);
        }
    }

    if(ATOMIC_QUEUE_LIKELY(!params->options.no_variant_2())) {
        if(ATOMIC_QUEUE_LIKELY(!params->options.no_variant_a())) {
            if(ATOMIC_QUEUE_LIKELY(!params->options.no_spsc()))
                time_throughput_spsc("AtomicQueue2", params, Type<SPSC::AtomicQueue2>{});
            time_throughput_mpmc("AtomicQueue2", params, Type<MPMC::AtomicQueue2>{}, 2);

            if(ATOMIC_QUEUE_LIKELY(!params->options.no_spsc()))
                time_throughput_spsc("OptimistAtomicQueue2", params, Type<SPSC::OptimistAtomicQueue2>{});
            time_throughput_mpmc("OptimistAtomicQueue2", params, Type<MPMC::OptimistAtomicQueue2>{}, 2);
        }

        if(ATOMIC_QUEUE_LIKELY(!params->options.no_variant_b())) {
            if(ATOMIC_QUEUE_LIKELY(!params->options.no_spsc()))
                time_throughput_spsc("AtomicQueueB2", params, Type<SPSC::AtomicQueueB2>{});
            time_throughput_mpmc("AtomicQueueB2", params, Type<MPMC::AtomicQueueB2>{}, 2);

            if(ATOMIC_QUEUE_LIKELY(!params->options.no_spsc()))
                time_throughput_spsc("OptimistAtomicQueueB2", params, Type<SPSC::OptimistAtomicQueueB2>{});
            time_throughput_mpmc("OptimistAtomicQueueB2", params, Type<MPMC::OptimistAtomicQueueB2>{}, 2);
        }
    }

    std::puts("\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
ATOMIC_QUEUE_NOINLINE void ping_pong_receiver(SharedState* ctx, ThreadState* thread) {
    Queue* const q1 = static_cast<Queue*>(ctx->queue0);
    Queue* const q2 = static_cast<Queue*>(ctx->queue1);

    [[maybe_unused]] region_guard_t<Queue> guard;
    ConsumerOf<Queue> consumer_q1{*q1};
    ProducerOf<Queue> producer_q2{*q2};

    ctx->barrier.countdown();
    thread->times.set(0);

    unsigned n;
    do {
        n = consumer_q1.pop(*q1) - 1;
        producer_q2.push(*q2, n);
    } while(ATOMIC_QUEUE_LIKELY(n > 1));

    thread->times.set(1); // std::memory_order_seq_cst;
}

template<class Queue>
ATOMIC_QUEUE_NOINLINE void ping_pong_sender(SharedState* ctx, ThreadState* thread) {
    Queue* const q1 = static_cast<Queue*>(ctx->queue0);
    Queue* const q2 = static_cast<Queue*>(ctx->queue1);

    [[maybe_unused]] region_guard_t<Queue> guard;
    ProducerOf<Queue> producer_q1{*q1};
    ConsumerOf<Queue> consumer_q2{*q2};
    unsigned n = ctx->n_msg;

    ctx->barrier.countdown();
    thread->times.set(0);

    do {
        producer_q1.push(*q1, n);
        n = consumer_q2.pop(*q2);
    } while(as_signed(--n) > 0);

    thread->times.set(1); // std::memory_order_seq_cst;
}

template<class Queue>
ATOMIC_QUEUE_INLINE cycles_t time_ping_pong_once(Params const* params, unsigned const (&cpus)[2]) {
    auto ctx = HugePages::instance->create_unique_ptr<SharedState2>(params, cpus);
    auto sender0 = ctx->reuse_this_thread(); // This thread#0 is the sender.

    ContextOf<Queue> const queue_ctx{1, 1};
    auto q1 = HugePages::instance->create_unique_ptr<Queue>(queue_ctx);
    auto q2 = HugePages::instance->create_unique_ptr<Queue>(queue_ctx);
    ctx->queue0 = q1.get();
    ctx->queue1 = q2.get();

    ctx->create_thread(ping_pong_receiver<Queue>);
    ping_pong_sender<Queue>(ctx.get(), sender0);
    ctx->join();

    return ctx->total_time();
}

template<class Queue>
ATOMIC_QUEUE_NOINLINE void time_ping_pong(char const* name, Params const* params) {
    // Select the best times of RUNS runs.
    cycles_t shortest_total_time = CYCLES_MAX;

    // Ping-pong between the first available CPU and every othery next power-of-2 to find its SMT sibling, if any.
    auto& hw_thread_ids = params->hw_thread_ids;
    unsigned const n_cpus = hw_thread_ids.size();
    for(unsigned cpu2 = 1; cpu2 < n_cpus; cpu2 *= 2) {
        unsigned const cpus[2] = {hw_thread_ids[0], hw_thread_ids[cpu2]};
        for(unsigned run = RUNS; run--;) {
            auto total_time = time_ping_pong_once<Queue>(params, cpus);
            if(shortest_total_time > total_time)
                shortest_total_time = total_time;

            HugePages::instance->check_huge_pages_leaks(name);
        }
    }

    auto round_trip_time = to_seconds(shortest_total_time * 2) / params->n_msg;
    printf("%32s: %.9f sec/round-trip\n", name, round_trip_time);
}

void run_ping_pong_benchmarks(Params const* params) {
    printf("---- Running ping-pong benchmarks with 2 CPUs (lower is better) ----\n");

    // This benchmark doesn't require queue capacity greater than 1, however, capacity of 1 elides
    // some instructions completely because of (x % 1) is always 0. Use something greater than 1 to
    // preclude aggressive optimizations.
    constexpr unsigned SIZE = 8;

    // The reference.
    time_ping_pong<BoostSpScAdapter<boost::lockfree::spsc_queue<unsigned, boost::lockfree::capacity<SIZE>>>>(
        "boost::lockfree::spsc_queue", params);

    if(ATOMIC_QUEUE_LIKELY(!params->options.minimal())) {
        time_ping_pong<BoostQueueAdapter<boost::lockfree::queue<unsigned, BoostAllocator, boost::lockfree::capacity<SIZE>>>>(
            "boost::lockfree::queue", params);

        // time_ping_pong<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, TicketSpinlock>>>("TicketSpinlock", hp, hw_thread_ids);
        // run_ping_pong_benchmark<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, UnfairSpinlock>>>("UnfairSpinlock", hp, hw_thread_ids);
        // run_ping_pong_benchmark<RetryDecorator<AtomicQueueSpinlockHle<unsigned, SIZE>>>("SpinlockHle");

        time_ping_pong<MoodyCamelReaderWriterQueue<unsigned, SIZE>>("moodycamel::ReaderWriterQueue", params);
        time_ping_pong<MoodyCamelQueue<unsigned, SIZE>>("moodycamel::ConcurrentQueue", params);

        time_ping_pong<RetryDecorator<AtomicQueueSpinlock<unsigned, SIZE>>>("pthread_spinlock", params);
        time_ping_pong<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, std::mutex>>>("std::mutex", params);
        time_ping_pong<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, tbb::spin_mutex>>>("tbb::spin_mutex", params);
        // run_ping_pong_benchmark<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, AdaptiveMutex>>>("adaptive_mutex", params);
        // run_ping_pong_benchmark<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, tbb::speculative_spin_mutex>>>("tbb::speculative_spin_mutex", params);
        time_ping_pong<TbbAdapter<tbb::concurrent_bounded_queue<unsigned>, SIZE>>("tbb::concurrent_bounded_queue", params);

        time_ping_pong<XeniumQueueAdapter<xenium::michael_scott_queue<unsigned, xenium::policy::reclaimer<Reclaimer>>>>("xenium::michael_scott_queue", params);
        time_ping_pong<XeniumQueueAdapter<xenium::ramalhete_queue<unsigned, xenium::policy::reclaimer<Reclaimer>>>>("xenium::ramalhete_queue", params);
        time_ping_pong<RetryDecorator<CapacityArgAdaptor<xenium::vyukov_bounded_queue<unsigned>, SIZE>>>("xenium::vyukov_bounded_queue", params);
    }

    // Use MAXIMIZE_THROUGHPUT=false for better latency.
    using SPSC = QueueTypes<SIZE, true, false, false>;

    if(ATOMIC_QUEUE_LIKELY(!params->options.no_variant_1())) {
        if(ATOMIC_QUEUE_LIKELY(!params->options.no_variant_a())) {
            time_ping_pong<SPSC::AtomicQueue>("AtomicQueue", params);
            time_ping_pong<SPSC::OptimistAtomicQueue>("OptimistAtomicQueue", params);
        }

        if(ATOMIC_QUEUE_LIKELY(!params->options.no_variant_b())) {
            time_ping_pong<SPSC::AtomicQueueB>("AtomicQueueB", params);
            time_ping_pong<SPSC::OptimistAtomicQueueB>("OptimistAtomicQueueB", params);
        }
    }

    if(ATOMIC_QUEUE_LIKELY(!params->options.no_variant_2())) {
        if(ATOMIC_QUEUE_LIKELY(!params->options.no_variant_a())) {
            time_ping_pong<SPSC::AtomicQueue2>("AtomicQueue2", params);
            time_ping_pong<SPSC::OptimistAtomicQueue2>("OptimistAtomicQueue2", params);
        }

        if(ATOMIC_QUEUE_LIKELY(!params->options.no_variant_b())) {
            time_ping_pong<SPSC::AtomicQueueB2>("AtomicQueueB2", params);
            time_ping_pong<SPSC::OptimistAtomicQueueB2>("OptimistAtomicQueueB2", params);
        }
    }

    std::puts("\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main() {
    Params params;

    std::setlocale(LC_NUMERIC, ""); // Enable thousand separator, if set in user's locale.

    TSC_TO_SECONDS = 1e-9 / cpu_base_frequency();

    auto const cpu_topology = get_available_cpu_topology_info();
    log_cpus(cpu_topology);
    if(cpu_topology.size() < 2)
        throw std::runtime_error("A CPU with at least 2 hardware threads is required.");

    params.hw_thread_ids = hw_thread_id(cpu_topology); // Sorted by hw_thread_id.
    set_thread_affinity(params.hw_thread_ids[0]); // Pin the main thread#0 to CPU#0 prior to allocating memory.

    size_t constexpr MB = 1024 * 1024;
    HugePages hp(HugePages::PAGE_1GB, 32 * MB); // Try allocating a 1GB huge page to minimize TLB misses.
    HugePages::instance = &hp;

    if(!params.options.no_throughput())
        run_throughput_benchmarks(&params);
    if(!params.options.no_ping_pong())
        run_ping_pong_benchmarks(&params);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
