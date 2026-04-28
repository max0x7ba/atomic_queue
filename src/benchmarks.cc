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

using namespace ::atomic_queue;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

struct BenchmarkOptions : EnvBits64 {
    ATOMIC_QUEUE_INLINE constexpr auto       minimal() const noexcept { return bits & 1; };

    ATOMIC_QUEUE_INLINE constexpr auto  no_ping_pong() const noexcept { return bits & 2; };
    ATOMIC_QUEUE_INLINE constexpr auto no_throughput() const noexcept { return bits & 4; };

    ATOMIC_QUEUE_INLINE constexpr auto  no_variant_a() const noexcept { return bits & 8; };
    ATOMIC_QUEUE_INLINE constexpr auto  no_variant_b() const noexcept { return bits & 16; };
    ATOMIC_QUEUE_INLINE constexpr auto  no_variant_1() const noexcept { return bits & 32; };
    ATOMIC_QUEUE_INLINE constexpr auto  no_variant_2() const noexcept { return bits & 64; };
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T>
using Type = std::common_type<T>; // Similar to boost::type<>.

using sum_t = long long;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class P>
struct Range {
    P p,q;
    auto begin() const noexcept { return p; }
    auto end() const noexcept { return q; }
};

template<class P>
Range<P> as_range(P p, P q) noexcept {
    return {p,q};
}

template<class P>
Range<P> as_range(P p, size_t n) noexcept {
    return {p, p +n };
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using cycles_t = decltype(__builtin_ia32_rdtsc());
static_assert(std::is_unsigned<cycles_t>::value);

using icycles_t = std::make_signed<cycles_t>::type; // Signed integers convert into double with one AVX instruction, unlike unsigned.
cycles_t constexpr CYCLES_MAX = -1;

ATOMIC_QUEUE_INLINE static cycles_t cycles() noexcept {
    // If software requires RDTSC to be executed only after all previous instructions have executed and all previous loads are
    // globally visible, it can execute LFENCE immediately before RDTSC.
    _mm_lfence();
    return __builtin_ia32_rdtsc();
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

void check_huge_pages_leaks(char const* name, HugePages& hp) {
    if(!hp.empty()) {
        std::fprintf(stderr, "%s: %zu bytes of HugePages memory leaked.\n", name, hp.used());
        hp.reset();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// According to my benchmarking, it looks like the best performance is achieved with the following parameters:
// * For SPSC: SPSC=true,  MINIMIZE_CONTENTION=false, MAXIMIZE_THROUGHPUT=false.
// * For MPMC: SPSC=false, MINIMIZE_CONTENTION=true,  MAXIMIZE_THROUGHPUT=true.
// However, I am not sure that conflating these 3 parameters into 1 would be the right thing for every scenario.
template<unsigned SIZE, bool SPSC, bool MINIMIZE_CONTENTION, bool MAXIMIZE_THROUGHPUT>
struct QueueTypes {
    using T = unsigned;

    // For atomic elements only.
    using AtomicQueue =                                Type<RetryDecorator<atomic_queue::AtomicQueue<T, SIZE, T{}, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, false, SPSC>>>;
    using OptimistAtomicQueue =                                       Type<atomic_queue::AtomicQueue<T, SIZE, T{}, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, false, SPSC>>;
    using AtomicQueueB =        Type<RetryDecorator<CapacityArgAdaptor<atomic_queue::AtomicQueueB<T, Allocator, T{}, MAXIMIZE_THROUGHPUT, false, SPSC>, SIZE>>>;
    using OptimistAtomicQueueB =               Type<CapacityArgAdaptor<atomic_queue::AtomicQueueB<T, Allocator, T{}, MAXIMIZE_THROUGHPUT, false, SPSC>, SIZE>>;

    // For non-atomic elements.
    using AtomicQueue2 =                         Type<RetryDecorator<atomic_queue::AtomicQueue2<T, SIZE, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, false, SPSC>>>;
    using OptimistAtomicQueue2 =                                Type<atomic_queue::AtomicQueue2<T, SIZE, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, false, SPSC>>;
    using AtomicQueueB2 = Type<RetryDecorator<CapacityArgAdaptor<atomic_queue::AtomicQueueB2<T, Allocator, MAXIMIZE_THROUGHPUT, false, SPSC>, SIZE>>>;
    using OptimistAtomicQueueB2 =        Type<CapacityArgAdaptor<atomic_queue::AtomicQueueB2<T, Allocator, MAXIMIZE_THROUGHPUT, false, SPSC>, SIZE>>;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T>
struct copyable_atomic : std::atomic<T> {
    using B = std::atomic<T>;
    using B::B;

    copyable_atomic(copyable_atomic const& b) noexcept {
        details::copy_relaxed(*this,b);
    }

    copyable_atomic(copyable_atomic&& b) noexcept {
        details::copy_relaxed(*this,b);
    }

    copyable_atomic& operator=(copyable_atomic const& b) noexcept {
        details::copy_relaxed(*this, b);
        return *this;
    }

    copyable_atomic& operator=(copyable_atomic&& b) noexcept {
        details::copy_relaxed(*this, b);
        return *this;
    }

    void swap(copyable_atomic& b) noexcept {
        details::swap_relaxed(*this,b);
    }

    friend void swap(copyable_atomic& a, copyable_atomic& b) noexcept {
        a.swap(b);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Times {
    enum { N_TIMES = 2 };
    copyable_atomic<cycles_t> t[N_TIMES];

    ATOMIC_QUEUE_INLINE void set(unsigned i) noexcept {
        auto* p = t + i; // Resolve the address into a register before cycles.
        auto now = cycles();
        p->store(now, std::memory_order_seq_cst);
    }

    ATOMIC_QUEUE_INLINE cycles_t get(unsigned i) const noexcept {
        return t[i].load(X);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ThreadState {
    alignas(CACHE_LINE_SIZE)
    Times times;
    copyable_atomic<sum_t> sum;
    std::thread thread;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SharedState {
    // These remain constant.
    alignas(CACHE_LINE_SIZE)
    unsigned const n_msg;
    unsigned const n_threads;

    void* queue0 = 0;
    void* queue1 = 0;

    ThreadState* const threads;
    unsigned const* ATOMIC_QUEUE_RESTRICT const hw_thread_ids;

    unsigned thread_idx = 0;

    // These are modified at the start.
    alignas(CACHE_LINE_SIZE)
    Barrier2 barrier;

    ATOMIC_QUEUE_INLINE SharedState(unsigned N, unsigned thread_count, unsigned const* ATOMIC_QUEUE_RESTRICT hw_thread_ids, ThreadState* consumer_sums) noexcept
        : n_msg(N)
        , n_threads(thread_count)
        , threads(consumer_sums)
        , hw_thread_ids{hw_thread_ids}
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

    ATOMIC_QUEUE_NOINLINE cycles_t max_duration() const noexcept {
        cycles_t t0 = CYCLES_MAX, t1 = 0;
        for(auto& thr : as_range(threads, n_threads * 2)) {
            t0 = std::min(t0, thr.times.get(0));
            t1 = std::max(t1, thr.times.get(1));
        }

        if(ATOMIC_QUEUE_UNLIKELY(t0 == CYCLES_MAX || !t1)) // Must never happen.
            std::abort();

        return t1 - t0;
    }
};

struct SharedState2 : SharedState {
    ThreadState threads2[2];

    template<class... Args>
    ATOMIC_QUEUE_INLINE SharedState2(Args... args)
        : SharedState(args..., threads2)
    {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
ATOMIC_QUEUE_NOINLINE void throughput_producer(SharedState* ctx, ThreadState* thread) {
    region_guard_t<Queue> guard;
    Queue* const queue = static_cast<Queue*>(ctx->queue0);
    ProducerOf<Queue> producer{*queue};
    unsigned n = ctx->n_msg;

    ctx->barrier.countdown();
    thread->times.set(0);

    do
        producer.push(*queue, n);
    while(--n);

    thread->sum.store(-1, std::memory_order_seq_cst);
    thread->times.set(1);
}

template<class Queue>
ATOMIC_QUEUE_NOINLINE void throughput_consumer(SharedState* ctx, ThreadState* thread) {
    region_guard_t<Queue> guard;
    Queue* const queue = static_cast<Queue*>(ctx->queue0);
    ConsumerOf<Queue> consumer{*queue};
    sum_t sum = 0;
    unsigned n;

    ctx->barrier.countdown();
    thread->times.set(0);

    do {
        n = consumer.pop(*queue);
        sum += n; // Includes stop value.
    } while(n > 1);

    thread->sum.store(sum, std::memory_order_seq_cst);
    thread->times.set(1);
}

template<class Queue>
ATOMIC_QUEUE_INLINE cycles_t time_throughput_once(HugePages& hp, std::vector<unsigned> const& hw_thread_ids, unsigned N, unsigned thread_count, bool alternative_placement, ThreadState* consumer_sums) {
    auto ctx = hp.create_unique_ptr<SharedState>(N, thread_count, hw_thread_ids.data(), consumer_sums);
    auto queue = hp.create_unique_ptr<Queue>(ContextOf<Queue>{thread_count, thread_count});
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

    return ctx->max_duration();
}

template<class Queue>
ATOMIC_QUEUE_NOINLINE void time_throughput(char const* name, HugePages& hp, std::vector<unsigned> const& hw_thread_ids, unsigned M, unsigned n_thread_min, unsigned n_thread_max) {
    int constexpr RUNS = 3;
    using ThreadStates = std::vector<ThreadState, HugePageAllocator<ThreadState>>;

    for(unsigned threads = n_thread_min; threads <= n_thread_max; ++threads) {
        unsigned const N = M / threads;
        sum_t const expected_sum = (N + 1) / 2. * N;
        double const expected_sum_inv = 1. / expected_sum;

        for(bool alternative_placement : {false, true}) {
            cycles_t min_time = CYCLES_MAX;

            for(unsigned run = RUNS; run--;) {
                ThreadStates consumer_sums(threads * 2);
                cycles_t time = time_throughput_once<Queue>(hp, hw_thread_ids, N, threads, alternative_placement, consumer_sums.data());
                min_time = std::min(min_time, time);

                // Calculate the checksum.
                sum_t total_sum = 0;
                for(unsigned i = 0, j = 0; j < threads * 2; ++j) {
                    auto consumer_sum = consumer_sums[j].sum.load(X);
                    if(consumer_sum != -1) {
                        total_sum += consumer_sum;
                        // Verify that no consumer was starved.
                        auto consumer_sum_frac = consumer_sum * expected_sum_inv;
                        if(consumer_sum_frac < .1)
                            std::fprintf(stderr, "%s: producers: %u: consumer %u received too few messages: %.2lf%% of expected.\n",
                                         name, threads, i, consumer_sum_frac);
                        ++i;
                    }
                }

                consumer_sums = ThreadStates(); // Deallocate memory.
                check_huge_pages_leaks(name, hp);

                // Verify that all messages were received exactly once: no duplicates, no omissions.
                if(auto diff = total_sum - expected_sum * threads)
                    std::fprintf(stderr, "%s: wrong checksum error: producers: %u, expected_sum: %'lld, diff: %'lld.\n",
                                 name, threads, expected_sum * threads, diff);
            }

            double min_seconds = to_seconds(min_time);
            unsigned msg_per_sec = N * threads / min_seconds;
            std::printf("%32s,%2u,%c: %'11u msg/sec\n", name, threads, alternative_placement ? 'i' : 's', msg_per_sec);
        }
    }
}

constexpr int N_TROUGHPUT_MESSAGES = 1'000'000;

template<class Queue>
ATOMIC_QUEUE_INLINE void time_throughput_mpmc(char const* name, HugePages& hp, std::vector<unsigned> const& hw_thread_ids, Type<Queue>, unsigned thread_count_min = 1) {
    unsigned const thread_count_max = hw_thread_ids.size() / 2;
    time_throughput<Queue>(name, hp, hw_thread_ids, N_TROUGHPUT_MESSAGES, thread_count_min, thread_count_max);
}

template<class... Args>
ATOMIC_QUEUE_INLINE void time_throughput_spsc(char const* name, HugePages& hp, std::vector<unsigned> const& hw_thread_ids,
                                   Type<BoostSpScAdapter<boost::lockfree::spsc_queue<Args...>>>) {
    using Queue = BoostSpScAdapter<boost::lockfree::spsc_queue<Args...>>;
    time_throughput<Queue>(name, hp, hw_thread_ids, N_TROUGHPUT_MESSAGES, 1, 1); // spsc_queue can only handle 1 producer and 1 consumer.
}

template<class Queue>
ATOMIC_QUEUE_INLINE void time_throughput_spsc(char const* name, HugePages& hp, std::vector<unsigned> const& hw_thread_ids, Type<Queue>) {
    time_throughput<Queue>(name, hp, hw_thread_ids, N_TROUGHPUT_MESSAGES, 1, 1); // Special case for 1 producer and 1 consumer.
}

ATOMIC_QUEUE_NOINLINE void run_throughput_benchmarks(HugePages& hp, std::vector<CpuTopologyInfo> const& cpu_topology, BenchmarkOptions options) {
    auto hw_thread_ids = hw_thread_id(cpu_topology); // Sorted by hw_thread_id: avoid HT, same socket.
    std::printf("---- Running throughput benchmarks with up to %zu CPUs (higher is better) ----\n", hw_thread_ids.size() & -2);

    int constexpr SIZE = 65536;

    // The reference.
    time_throughput_spsc("boost::lockfree::spsc_queue", hp, hw_thread_ids,
                                  Type<BoostSpScAdapter<boost::lockfree::spsc_queue<unsigned, boost::lockfree::capacity<SIZE>>>>{});

    if(ATOMIC_QUEUE_LIKELY(!options.minimal())) {
        time_throughput_mpmc("boost::lockfree::queue", hp, hw_thread_ids,
                                      Type<BoostQueueAdapter<boost::lockfree::queue<unsigned, BoostAllocator, boost::lockfree::capacity<SIZE - 2>>>>{});

        // time_throughput_mpmc("TicketSpinlock", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, TicketSpinlock>>>{});
        // run_throughput_mpmc_benchmark("UnfairSpinlock", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, UnfairSpinlock>>>{});
        // run_throughput_mpmc_benchmark<RetryDecorator<AtomicQueueSpinlockHle<unsigned, SIZE>>>("SpinlockHle");

        time_throughput_spsc("moodycamel::ReaderWriterQueue", hp, hw_thread_ids, Type<MoodyCamelReaderWriterQueue<unsigned, SIZE>>{});
        time_throughput_mpmc("moodycamel::ConcurrentQueue", hp, hw_thread_ids, Type<MoodyCamelQueue<unsigned, SIZE>>{});

        time_throughput_mpmc("pthread_spinlock", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueueSpinlock<unsigned, SIZE>>>{});
        time_throughput_mpmc("std::mutex", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, std::mutex>>>{});
        time_throughput_mpmc("tbb::spin_mutex", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, tbb::spin_mutex>>>{});
        // run_throughput_mpmc_benchmark("adaptive_mutex", hp, hw_thread_ids, Type<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, AdaptiveMutex>>>{});
        // run_throughput_mpmc_benchmark("tbb::speculative_spin_mutex", hp, hw_thread_ids,
        //                               Type<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, tbb::speculative_spin_mutex>>>{});
        time_throughput_mpmc("tbb::concurrent_bounded_queue", hp, hw_thread_ids, Type<TbbAdapter<tbb::concurrent_bounded_queue<unsigned>, SIZE>>{});

        time_throughput_mpmc("xenium::michael_scott_queue", hp, hw_thread_ids,
            Type<XeniumQueueAdapter<xenium::michael_scott_queue<unsigned, xenium::policy::reclaimer<Reclaimer>>>>{});
        time_throughput_mpmc("xenium::ramalhete_queue", hp, hw_thread_ids,
            Type<XeniumQueueAdapter<xenium::ramalhete_queue<unsigned, xenium::policy::reclaimer<Reclaimer>>>>{});
        time_throughput_mpmc("xenium::vyukov_bounded_queue", hp, hw_thread_ids,
            Type<RetryDecorator<CapacityArgAdaptor<xenium::vyukov_bounded_queue<unsigned>, SIZE>>>{});
    }

    using SPSC = QueueTypes<SIZE, true, false, false>;
    using MPMC = QueueTypes<SIZE, false, true, true>; // Enable MAXIMIZE_THROUGHPUT for 2 or more producers/consumers.

    if(ATOMIC_QUEUE_LIKELY(!options.no_variant_1())) {
        if(ATOMIC_QUEUE_LIKELY(!options.no_variant_a())) {
            time_throughput_spsc("AtomicQueue", hp, hw_thread_ids, SPSC::AtomicQueue{});
            time_throughput_mpmc("AtomicQueue", hp, hw_thread_ids, MPMC::AtomicQueue{}, 2);
            time_throughput_spsc("OptimistAtomicQueue", hp, hw_thread_ids, SPSC::OptimistAtomicQueue{});
            time_throughput_mpmc("OptimistAtomicQueue", hp, hw_thread_ids, MPMC::OptimistAtomicQueue{}, 2);
        }

        if(ATOMIC_QUEUE_LIKELY(!options.no_variant_b())) {
            time_throughput_spsc("AtomicQueueB", hp, hw_thread_ids, SPSC::AtomicQueueB{});
            time_throughput_mpmc("AtomicQueueB", hp, hw_thread_ids, MPMC::AtomicQueueB{}, 2);
            time_throughput_spsc("OptimistAtomicQueueB", hp, hw_thread_ids, SPSC::OptimistAtomicQueueB{});
            time_throughput_mpmc("OptimistAtomicQueueB", hp, hw_thread_ids, MPMC::OptimistAtomicQueueB{}, 2);
        }
    }

    if(ATOMIC_QUEUE_LIKELY(!options.no_variant_2())) {
        if(ATOMIC_QUEUE_LIKELY(!options.no_variant_a())) {
            time_throughput_spsc("AtomicQueue2", hp, hw_thread_ids, SPSC::AtomicQueue2{});
            time_throughput_mpmc("AtomicQueue2", hp, hw_thread_ids, MPMC::AtomicQueue2{}, 2);

            time_throughput_spsc("OptimistAtomicQueue2", hp, hw_thread_ids, SPSC::OptimistAtomicQueue2{});
            time_throughput_mpmc("OptimistAtomicQueue2", hp, hw_thread_ids, MPMC::OptimistAtomicQueue2{}, 2);
        }

        if(ATOMIC_QUEUE_LIKELY(!options.no_variant_b())) {
            time_throughput_spsc("AtomicQueueB2", hp, hw_thread_ids, SPSC::AtomicQueueB2{});
            time_throughput_mpmc("AtomicQueueB2", hp, hw_thread_ids, MPMC::AtomicQueueB2{}, 2);

            time_throughput_spsc("OptimistAtomicQueueB2", hp, hw_thread_ids, SPSC::OptimistAtomicQueueB2{});
            time_throughput_mpmc("OptimistAtomicQueueB2", hp, hw_thread_ids, MPMC::OptimistAtomicQueueB2{}, 2);
        }
    }

    std::puts("\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
ATOMIC_QUEUE_NOINLINE void ping_pong_receiver(SharedState* ctx, ThreadState* thread) {
    region_guard_t<Queue> guard;
    Queue* const q1 = static_cast<Queue*>(ctx->queue0);
    Queue* const q2 = static_cast<Queue*>(ctx->queue1);
    ConsumerOf<Queue> consumer_q1{*q1};
    ProducerOf<Queue> producer_q2{*q2};

    ctx->barrier.countdown();
    thread->times.set(0);

    unsigned n;
    do {
        n = consumer_q1.pop(*q1) - 1;
        producer_q2.push(*q2, n);
    } while(ATOMIC_QUEUE_LIKELY(n > 1));

    thread->sum.store(-1, std::memory_order_seq_cst);
    thread->times.set(1);
}

template<class Queue>
ATOMIC_QUEUE_NOINLINE void ping_pong_sender(SharedState* ctx, ThreadState* thread) {
    region_guard_t<Queue> guard;
    Queue* const q1 = static_cast<Queue*>(ctx->queue0);
    Queue* const q2 = static_cast<Queue*>(ctx->queue1);
    ProducerOf<Queue> producer_q1{*q1};
    ConsumerOf<Queue> consumer_q2{*q2};
    unsigned n = ctx->n_msg;

    ctx->barrier.countdown();
    thread->times.set(0);

    do {
        producer_q1.push(*q1, n);
        n = consumer_q2.pop(*q2);
    } while(as_signed(--n) > 0);

    thread->sum.store(-1, std::memory_order_seq_cst);
    thread->times.set(1);
}

template<class Queue>
ATOMIC_QUEUE_INLINE cycles_t time_ping_pong_once(unsigned N, HugePages& hp, unsigned const (&cpus)[2]) {
    auto ctx = hp.create_unique_ptr<SharedState2>(N, 1u, cpus);
    auto sender0 = ctx->reuse_this_thread(); // This thread#0 is the sender.

    // set_thread_affinity(cpus[0]); // This thread#0 is the sender.
    // set_default_thread_affinity(cpus[1]);

    ContextOf<Queue> const queue_ctx{1, 1};
    auto q1 = hp.create_unique_ptr<Queue>(queue_ctx);
    auto q2 = hp.create_unique_ptr<Queue>(queue_ctx);
    ctx->queue0 = q1.get();
    ctx->queue1 = q2.get();

    ctx->create_thread(ping_pong_receiver<Queue>);
    ping_pong_sender<Queue>(ctx.get(), sender0);
    ctx->join();

    return ctx->max_duration();
}

template<class Queue>
ATOMIC_QUEUE_NOINLINE void time_ping_pong(char const* name, HugePages& hp, std::vector<unsigned> const& hw_thread_ids) {
    int constexpr N_PING_PONG_MESSAGES = 1'000'000;
    int constexpr RUNS = 3;

    // Select the best times of RUNS runs.
    cycles_t best_duration = CYCLES_MAX;

    // Ping-pong between the first available CPU and every othery next power-of-2 to find its SMT sibling, if any.
    unsigned const n_cpus = hw_thread_ids.size();
    for(unsigned cpu2 = 1; cpu2 < n_cpus; cpu2 *= 2) {
        unsigned const cpus[2] = {hw_thread_ids[0], hw_thread_ids[cpu2]};
        for(unsigned run = RUNS; run--;) {
            auto duration = time_ping_pong_once<Queue>(N_PING_PONG_MESSAGES, hp, cpus);
            if(duration < best_duration)
                best_duration = duration;

            check_huge_pages_leaks(name, hp);
        }
    }

    auto round_trip_time = to_seconds(best_duration) / N_PING_PONG_MESSAGES;
    std::printf("%32s: %.9f sec/round-trip\n", name, round_trip_time);
}

void run_ping_pong_benchmarks(HugePages& hp, std::vector<CpuTopologyInfo> const& cpu_topology, BenchmarkOptions options) {
    auto hw_thread_ids = hw_thread_id(cpu_topology); // Sorted by hw_thread_id: avoid HT, same socket.
    std::printf("---- Running ping-pong benchmarks with 2 CPUs (lower is better) ----\n");

    // This benchmark doesn't require queue capacity greater than 1, however, capacity of 1 elides
    // some instructions completely because of (x % 1) is always 0. Use something greater than 1 to
    // preclude aggressive optimizations.
    constexpr unsigned SIZE = 8;

    // The reference.
    time_ping_pong<BoostSpScAdapter<boost::lockfree::spsc_queue<unsigned, boost::lockfree::capacity<SIZE>>>>("boost::lockfree::spsc_queue", hp, hw_thread_ids);

    if(ATOMIC_QUEUE_LIKELY(!options.minimal())) {
        time_ping_pong<BoostQueueAdapter<boost::lockfree::queue<unsigned, BoostAllocator, boost::lockfree::capacity<SIZE>>>>("boost::lockfree::queue", hp,
                                                                                                                                      hw_thread_ids);

        // time_ping_pong<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, TicketSpinlock>>>("TicketSpinlock", hp, hw_thread_ids);
        // run_ping_pong_benchmark<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, UnfairSpinlock>>>("UnfairSpinlock", hp, hw_thread_ids);
        // run_ping_pong_benchmark<RetryDecorator<AtomicQueueSpinlockHle<unsigned, SIZE>>>("SpinlockHle");

        time_ping_pong<MoodyCamelReaderWriterQueue<unsigned, SIZE>>("moodycamel::ReaderWriterQueue", hp, hw_thread_ids);
        time_ping_pong<MoodyCamelQueue<unsigned, SIZE>>("moodycamel::ConcurrentQueue", hp, hw_thread_ids);

        time_ping_pong<RetryDecorator<AtomicQueueSpinlock<unsigned, SIZE>>>("pthread_spinlock", hp, hw_thread_ids);
        time_ping_pong<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, std::mutex>>>("std::mutex", hp, hw_thread_ids);
        time_ping_pong<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, tbb::spin_mutex>>>("tbb::spin_mutex", hp, hw_thread_ids);
        // run_ping_pong_benchmark<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, AdaptiveMutex>>>("adaptive_mutex", hp, hw_thread_ids);
        // run_ping_pong_benchmark<RetryDecorator<AtomicQueueMutex<unsigned, SIZE, tbb::speculative_spin_mutex>>>("tbb::speculative_spin_mutex", hp, hw_thread_ids);
        time_ping_pong<TbbAdapter<tbb::concurrent_bounded_queue<unsigned>, SIZE>>("tbb::concurrent_bounded_queue", hp, hw_thread_ids);

        time_ping_pong<XeniumQueueAdapter<xenium::michael_scott_queue<unsigned, xenium::policy::reclaimer<Reclaimer>>>>("xenium::michael_scott_queue", hp, hw_thread_ids);
        time_ping_pong<XeniumQueueAdapter<xenium::ramalhete_queue<unsigned, xenium::policy::reclaimer<Reclaimer>>>>("xenium::ramalhete_queue", hp, hw_thread_ids);
        time_ping_pong<RetryDecorator<CapacityArgAdaptor<xenium::vyukov_bounded_queue<unsigned>, SIZE>>>("xenium::vyukov_bounded_queue", hp, hw_thread_ids);
    }

    // Use MAXIMIZE_THROUGHPUT=false for better latency.
    using SPSC = QueueTypes<SIZE, true, false, false>;

    if(ATOMIC_QUEUE_LIKELY(!options.no_variant_1())) {
        if(ATOMIC_QUEUE_LIKELY(!options.no_variant_a())) {
            time_ping_pong<SPSC::AtomicQueue::type>("AtomicQueue", hp, hw_thread_ids);
            time_ping_pong<SPSC::OptimistAtomicQueue::type>("OptimistAtomicQueue", hp, hw_thread_ids);
        }

        if(ATOMIC_QUEUE_LIKELY(!options.no_variant_b())) {
            time_ping_pong<SPSC::AtomicQueueB::type>("AtomicQueueB", hp, hw_thread_ids);
            time_ping_pong<SPSC::OptimistAtomicQueueB::type>("OptimistAtomicQueueB", hp, hw_thread_ids);
        }
    }

    if(ATOMIC_QUEUE_LIKELY(!options.no_variant_2())) {
        if(ATOMIC_QUEUE_LIKELY(!options.no_variant_a())) {
            time_ping_pong<SPSC::AtomicQueue2::type>("AtomicQueue2", hp, hw_thread_ids);
            time_ping_pong<SPSC::OptimistAtomicQueue2::type>("OptimistAtomicQueue2", hp, hw_thread_ids);
        }

        if(ATOMIC_QUEUE_LIKELY(!options.no_variant_b())) {
            time_ping_pong<SPSC::AtomicQueueB2::type>("AtomicQueueB2", hp, hw_thread_ids);
            time_ping_pong<SPSC::OptimistAtomicQueueB2::type>("OptimistAtomicQueueB2", hp, hw_thread_ids);
        }
    }

    std::puts("\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void advise_hugeadm_1GB() {
    std::fprintf(stderr, "Warning: Failed to allocate 1GB huge pages. Run \"sudo hugeadm --pool-pages-min 1GB:1 --pool-pages-max 1GB:1\".\n");
}

void advise_hugeadm_2MB() {
    std::fprintf(stderr, "Warning: Failed to allocate 2MB huge pages. Run \"sudo hugeadm --pool-pages-min 2MB:16 --pool-pages-max 2MB:16\".\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main() {
    BenchmarkOptions const options{"AQB"};

    std::setlocale(LC_NUMERIC, ""); // Enable thousand separator, if set in user's locale.

    TSC_TO_SECONDS = 1e-9 / cpu_base_frequency();

    auto const cpu_topology = get_available_cpu_topology_info();
    log_cpus(cpu_topology);
    if(cpu_topology.size() < 2)
        throw std::runtime_error("A CPU with at least 2 hardware threads is required.");

    set_thread_affinity(cpu_topology[0].hw_thread_id); // Pin the main thread#0 to CPU#0 prior to allocating memory.

    HugePages::warn_no_1GB_pages = advise_hugeadm_1GB;
    HugePages::warn_no_2MB_pages = advise_hugeadm_2MB;
    size_t constexpr MB = 1024 * 1024;
    HugePages hp(HugePages::PAGE_1GB, 32 * MB); // Try allocating a 1GB huge page to minimize TLB misses.
    HugePageAllocatorBase::hp = &hp;

    if(!options.no_throughput())
        run_throughput_benchmarks(hp, cpu_topology, options);
    if(!options.no_ping_pong())
        run_ping_pong_benchmarks(hp, cpu_topology, options);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
