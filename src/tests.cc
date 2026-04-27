/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#define BOOST_TEST_MODULE atomic_queue
#include <boost/test/unit_test.hpp>

#include "atomic_queue/atomic_queue.h"
#include "atomic_queue/barrier.h"
#include "benchmarks.h"

#include <boost/mpl/list.hpp>
#include <bitset>
#include <cstdint>
#include <thread>
#include <string>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace ::atomic_queue;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum { N_STRESS_MSG = 1000000 };
enum { STOP_MSG = -1 };
enum { CAPACITY = 4096 };

using stress_queues = boost::mpl::list<
    AtomicQueue<unsigned, CAPACITY>,
    AtomicQueue2<unsigned, CAPACITY>,
    AtomicQueue<unsigned, CAPACITY, 0u, true, true, false, true>,
    AtomicQueue2<unsigned, CAPACITY, true, true, false, true>,

    CapacityArgAdaptor<AtomicQueueB<unsigned>, CAPACITY>,
    CapacityArgAdaptor<AtomicQueueB2<unsigned>, CAPACITY>,
    CapacityArgAdaptor<AtomicQueueB<unsigned, std::allocator<unsigned>, 0u, true, false, true>, CAPACITY>,
    CapacityArgAdaptor<AtomicQueueB2<unsigned, std::allocator<unsigned>, true, false, true>, CAPACITY>,

    RetryDecorator<AtomicQueue<unsigned, CAPACITY>>,
    RetryDecorator<AtomicQueue2<unsigned, CAPACITY>>,
    RetryDecorator<AtomicQueue<unsigned, CAPACITY, 0u, true, true, false, true>>,
    RetryDecorator<AtomicQueue2<unsigned, CAPACITY, true, true, false, true>>,

    RetryDecorator<CapacityArgAdaptor<AtomicQueueB<unsigned>, CAPACITY>>,
    RetryDecorator<CapacityArgAdaptor<AtomicQueueB2<unsigned>, CAPACITY>>,
    RetryDecorator<CapacityArgAdaptor<AtomicQueueB<unsigned, std::allocator<unsigned>, 0u, true, false, true>, CAPACITY>>,
    RetryDecorator<CapacityArgAdaptor<AtomicQueueB2<unsigned, std::allocator<unsigned>, true, false, true>, CAPACITY>>
>;

// Check that all push'es are ever pop'ed once with multiple producer and multiple consumers.
BOOST_AUTO_TEST_CASE_TEMPLATE(stress, Queue, stress_queues) {
    enum {
        PRODUCERS = Queue::is_spsc() ? 1 : 3,
        CONSUMERS = Queue::is_spsc() ? 1 : 3
    };
    using T = typename Queue::value_type;

    Queue q;
    Barrier barrier;

    std::thread producers[PRODUCERS];
    for(unsigned i = 0; i < PRODUCERS; ++i)
        producers[i] = std::thread([&q, &barrier]() {
            barrier.wait();
            for(T n = N_STRESS_MSG; n; --n)
                q.push(n);
        });

    uint64_t results[CONSUMERS];
    std::thread consumers[CONSUMERS];
    for(unsigned i = 0; i < CONSUMERS; ++i)
        consumers[i] = std::thread([&q, &barrier, &r = results[i]]() {
            barrier.wait();
            uint64_t result = 0;
            for(T n; (n = q.pop()) != static_cast<T>(STOP_MSG);)
                result += n;
            r = result;
        });

    barrier.release(PRODUCERS + CONSUMERS);
    for(auto& t : producers)
        t.join();
    for(int i = CONSUMERS; i--;)
        q.push(STOP_MSG);
    for(auto& t : consumers)
        t.join();

    constexpr uint64_t expected_result = (N_STRESS_MSG + 1) / 2. * N_STRESS_MSG * PRODUCERS;
    constexpr uint64_t consumer_result_min = expected_result / CONSUMERS / 10;
    uint64_t result = 0;
    for(auto& r : results) {
        BOOST_WARN_GT(r, consumer_result_min); // Make sure a consumer didn't starve. False positives are possible here.
        result += r;
    }
    int64_t result_diff = result - expected_result;
    BOOST_CHECK_EQUAL(result_diff, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, class State>
struct test_stateful_allocator : std::allocator<T> {
    State state;
    test_stateful_allocator() = delete;

    // disambiguate constructor with std::nullptr_t
    // std::in_place available since C++17
    test_stateful_allocator(std::nullptr_t, const State& s) noexcept
        : state(s) {}

    test_stateful_allocator(const test_stateful_allocator& other) noexcept
        : std::allocator<T>(other), state(other.state) {}

    template<class U>
    test_stateful_allocator(const test_stateful_allocator<U, State>& other) noexcept
        : state(other.state) {}

    test_stateful_allocator& operator=(const test_stateful_allocator& other) noexcept {
        state = other.state;
        return *this;
    }

    ~test_stateful_allocator() noexcept = default;

    template<class U>
    struct rebind {
        using other = test_stateful_allocator<U, State>;
    };
};

// Required by boost-test
template<class T, class State>
std::ostream& operator<<(std::ostream& os, const test_stateful_allocator<T, State>& allocator) {
    return os << allocator.state;
}

template<class T1, class T2, class State>
bool operator==(const test_stateful_allocator<T1, State>& lhs, const test_stateful_allocator<T2, State>& rhs) {
    return lhs.state == rhs.state;
}

template<class T1, class T2, class State>
bool operator!=(const test_stateful_allocator<T1, State>& lhs, const test_stateful_allocator<T2, State>& rhs) {
    return !(lhs.state == rhs.state);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using move_only_element_queues = boost::mpl::list<
    AtomicQueue2<std::unique_ptr<int>, CAPACITY>,
    CapacityArgAdaptor<AtomicQueueB2<std::unique_ptr<int>>, CAPACITY>
>;

BOOST_AUTO_TEST_CASE_TEMPLATE(move_only_element, Queue, move_only_element_queues) {
    Queue q;

    BOOST_CHECK(q.was_empty());
    BOOST_CHECK_EQUAL(q.was_size(), 0u);
    std::unique_ptr<int> p{new int{1}};
    BOOST_REQUIRE(q.try_push(std::move(p)));
    BOOST_CHECK(!p);
    BOOST_CHECK(!q.was_empty());
    BOOST_CHECK_EQUAL(q.was_size(), 1u);

    p.reset(new int{2});
    q.push(std::move(p));
    BOOST_REQUIRE(!p);
    BOOST_CHECK(!q.was_empty());
    BOOST_CHECK_EQUAL(q.was_size(), 2u);

    BOOST_REQUIRE(q.try_pop(p));
    BOOST_REQUIRE(p.get());
    BOOST_CHECK_EQUAL(*p, 1);
    BOOST_CHECK(!q.was_empty());
    BOOST_CHECK_EQUAL(q.was_size(), 1u);

    p = q.pop();
    BOOST_REQUIRE(p.get());
    BOOST_CHECK_EQUAL(*p, 2);
    BOOST_CHECK(q.was_empty());
    BOOST_CHECK_EQUAL(q.was_size(), 0u);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(allocator_constructor_only_b) {
    using allocator_type = test_stateful_allocator<int, std::string>;
    const auto allocator = allocator_type(nullptr, "Capybara");

    AtomicQueueB<int, allocator_type> q(2, allocator);

    BOOST_CHECK_EQUAL(q.get_allocator(), allocator);
    auto q2 = std::move(q);
    BOOST_CHECK_EQUAL(q2.get_allocator(), allocator);
}

BOOST_AUTO_TEST_CASE(allocator_constructor_only_b2) {
    using allocator_type = test_stateful_allocator<std::unique_ptr<int>, std::string>;
    const auto allocator = allocator_type(nullptr, "Fox");

    AtomicQueueB2<std::unique_ptr<int>, allocator_type> q(2, allocator);

    BOOST_CHECK_EQUAL(q.get_allocator(), allocator);
    auto q2 = std::move(q);
    BOOST_CHECK_EQUAL(q2.get_allocator(), allocator);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using move_constructor_assignment_queues = boost::mpl::list<
    CapacityArgAdaptor<AtomicQueueB<int>, 2>,
    CapacityArgAdaptor<AtomicQueueB2<std::unique_ptr<int>>, 2>
>;

BOOST_AUTO_TEST_CASE_TEMPLATE(move_constructor_assignment, Queue, move_constructor_assignment_queues) {
    Queue q;
    auto const capacity = q.capacity();
    BOOST_CHECK_GE(capacity, 2);

    Queue q2 = std::move(q);
    BOOST_CHECK(!q.capacity());
    BOOST_CHECK_EQUAL(q2.capacity(), capacity);

    q = std::move(q2);
    BOOST_CHECK_EQUAL(q.capacity(), capacity);
    BOOST_CHECK(!q2.capacity());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(try_push_pop) {
    using Queue = atomic_queue::AtomicQueueB2<
      /* T = */ float,
      /* A = */ std::allocator<float>,
      /* MAXIMIZE_THROUGHPUT */ true,
      /* TOTAL_ORDER = */ true,
      /* SPSC = */ true
      >;

    constexpr unsigned CAPACITY = CACHE_LINE_SIZE * CACHE_LINE_SIZE;
    Queue q(CAPACITY);
    BOOST_CHECK_EQUAL(q.capacity(), CAPACITY);
    BOOST_CHECK(q.was_empty());
    BOOST_CHECK(!q.was_full());
    BOOST_CHECK_EQUAL(q.was_size(), 0u);

    // try_pop on empty queue must fail.
    float v = -1;
    BOOST_CHECK(!q.try_pop(v));
    BOOST_CHECK_EQUAL(v, -1);

    for(unsigned i = 1; i <= CAPACITY; ++i)
        BOOST_CHECK(q.try_push(i));

    BOOST_CHECK(!q.was_empty());
    BOOST_CHECK(q.was_full());
    BOOST_CHECK_EQUAL(q.was_size(), CAPACITY);

    for(unsigned i = 1; i <= CAPACITY; ++i)
        BOOST_CHECK(!q.try_push(i));

    BOOST_CHECK(!q.was_empty());
    BOOST_CHECK(q.was_full());
    BOOST_CHECK_EQUAL(q.was_size(), CAPACITY);

    for(unsigned i = 1; i <= CAPACITY; ++i) {
        BOOST_CHECK(q.try_pop(v));
        BOOST_CHECK_EQUAL(v, static_cast<float>(i));
    }

    BOOST_CHECK(q.was_empty());
    BOOST_CHECK(!q.was_full());
    BOOST_CHECK_EQUAL(q.was_size(), 0u);

    // try_pop on empty queue must fail again.
    v = -1;
    BOOST_CHECK(!q.try_pop(v));
    BOOST_CHECK_EQUAL(v, -1);
}

BOOST_AUTO_TEST_CASE(size) {
    atomic_queue::RetryDecorator<atomic_queue::AtomicQueueB2<float>> q(10);
    BOOST_CHECK_EQUAL(q.capacity(), CACHE_LINE_SIZE * CACHE_LINE_SIZE);
    BOOST_CHECK(q.was_empty());
    BOOST_CHECK(!q.was_full());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(power_of_2) {
    using atomic_queue::details::round_up_to_power_of_2;
    static_assert(round_up_to_power_of_2(0u) == 0u, "");
    static_assert(round_up_to_power_of_2(1u) == 1u, "");
    static_assert(round_up_to_power_of_2(2u) == 2u, "");
    static_assert(round_up_to_power_of_2(3u) == 4u, "");
    static_assert(round_up_to_power_of_2(127u) == 128u, "");
    static_assert(round_up_to_power_of_2(128u) == 128u, "");
    static_assert(round_up_to_power_of_2(129u) == 256u, "");
    static_assert(round_up_to_power_of_2(0x40000000u - 1) == 0x40000000u, "");
    static_assert(round_up_to_power_of_2(0x40000000u    ) == 0x40000000u, "");
    static_assert(round_up_to_power_of_2(0x40000000u + 1) == 0x80000000u, "");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

auto const bits0 = details::IndexBits<0>{};
auto const bits1 = details::IndexBits<1>{};
auto const bits2 = details::IndexBits<2>{};
auto const bits3 = details::IndexBits<3>{};
auto const bits4 = details::IndexBits<4>{};

using remap_index_fns = boost::mpl::list<
    details::RemapXor,
#ifdef __BMI__
    details::RemapBmi,
#endif
    details::RemapAnd
>;

BOOST_AUTO_TEST_CASE_TEMPLATE(remap_index, Remap, remap_index_fns) {
    details::Remap0<Remap> remap;

    // BITS=0 does no remapping.
    for(unsigned i = 256; i--;)
        BOOST_CHECK_EQUAL(remap(bits0, i), i);

    // Swap bit 0 with bit 1.
    BOOST_CHECK_EQUAL(remap(bits1, 0b00u), 0b00u);
    BOOST_CHECK_EQUAL(remap(bits1, 0b01u), 0b10u);
    BOOST_CHECK_EQUAL(remap(bits1, 0b10u), 0b01u);
    BOOST_CHECK_EQUAL(remap(bits1, 0b11u), 0b11u);
    // High bits are preserved.
    BOOST_CHECK_EQUAL(remap(bits1, 0b100u), 0b100u);
    BOOST_CHECK_EQUAL(remap(bits1, 0b101u), 0b110u);

    // Swap bits [0:1] with bits [2:3].
    BOOST_CHECK_EQUAL(remap(bits2, 0b0001u), 0b0100u);
    BOOST_CHECK_EQUAL(remap(bits2, 0b0100u), 0b0001u);
    BOOST_CHECK_EQUAL(remap(bits2, 0b0110u), 0b1001u);
    BOOST_CHECK_EQUAL(remap(bits2, 0b1111u), 0b1111u);

    // Swap bits [0:2] with bits [3:5].
    BOOST_CHECK_EQUAL(remap(bits3, 0b000'001u), 0b001'000u);
    BOOST_CHECK_EQUAL(remap(bits3, 0b001'000u), 0b000'001u);

    // remap_index is its own inverse: applying it twice yields the original index.
    unsigned constexpr size = 1024;
    unsigned constexpr poison_unused_msb = -size;
    for(unsigned i = size; i--;) {
        BOOST_CHECK_EQUAL(remap(bits1, remap(bits1, i, size)), i);
        BOOST_CHECK_EQUAL(remap(bits2, remap(bits2, i, size)), i);
        BOOST_CHECK_EQUAL(remap(bits3, remap(bits3, i, size)), i);
        BOOST_CHECK_EQUAL(remap(bits4, remap(bits4, i, size)), i);
        BOOST_CHECK_EQUAL(remap(bits1, remap(bits1, i | poison_unused_msb, size)), i);
        BOOST_CHECK_EQUAL(remap(bits2, remap(bits2, i | poison_unused_msb, size)), i);
        BOOST_CHECK_EQUAL(remap(bits3, remap(bits3, i | poison_unused_msb, size)), i);
        BOOST_CHECK_EQUAL(remap(bits4, remap(bits4, i | poison_unused_msb, size)), i);
    }

    // remap_index is a bijection over any power-of-2 range that covers the swapped bits.
    constexpr unsigned N = 256; // 8-bit index.
    std::bitset<N> seen;
    BOOST_REQUIRE(seen.none());
    for(unsigned i = N; i--;) {
        auto j = remap(bits3, i); // Swap bits [0:2] with bits [3:5] in the 8-bit index.
        BOOST_CHECK_LT(j, N);
        seen.set(j);
    }
    BOOST_CHECK_EQUAL(seen.count(), N);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
