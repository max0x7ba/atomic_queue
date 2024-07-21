/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#define BOOST_TEST_MODULE atomic_queue
#include <boost/test/unit_test.hpp>

#include "atomic_queue/atomic_queue.h"
#include "atomic_queue/atomic_queue_mutex.h"
#include "atomic_queue/barrier.h"

#include <cstdint>
#include <thread>
#include <string>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace ::atomic_queue;

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Check that all push'es are ever pop'ed once with multiple producer and multiple consumers.
template<class Queue>
void stress() {
    constexpr int PRODUCERS = 3;
    constexpr int CONSUMERS = 3;
    constexpr unsigned N = 1000000;

    Queue q;
    Barrier barrier;

    std::thread producers[PRODUCERS];
    for(unsigned i = 0; i < PRODUCERS; ++i)
        producers[i] = std::thread([&q, &barrier, N=N]() {
            barrier.wait();
            for(unsigned n = N; n; --n)
                q.push(n);
        });

    uint64_t results[CONSUMERS];
    std::thread consumers[CONSUMERS];
    for(unsigned i = 0; i < CONSUMERS; ++i)
        consumers[i] = std::thread([&q, &barrier, &r = results[i]]() {
            barrier.wait();
            uint64_t result = 0;
            for(;;) {
                unsigned n = q.pop();
                result += n;
                if(n == 1)
                    break;
            }
            r = result;
        });

    barrier.release(PRODUCERS + CONSUMERS);

    for(auto& t : producers)
        t.join();
    for(auto& t : consumers)
        t.join();

    constexpr uint64_t expected_result = (N + 1) / 2. * N;

    uint64_t result = 0;
    for(auto& r : results) {
        BOOST_WARN_GT(r, (expected_result / CONSUMERS) / 10); // Make sure a consumer didn't starve. False positives are possible here.
        result += r;
    }

    int64_t result_diff = result / CONSUMERS - expected_result;
    BOOST_CHECK_EQUAL(result_diff, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Q>
void test_unique_ptr_int(Q& q) {
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

constexpr unsigned CAPACITY = 1024;

BOOST_AUTO_TEST_CASE(stress_AtomicQueue) {
    stress<RetryDecorator<AtomicQueue<unsigned, CAPACITY>>>();
}

BOOST_AUTO_TEST_CASE(stress_BlockingAtomicQueue) {
    stress<AtomicQueue<unsigned, CAPACITY>>();
}

BOOST_AUTO_TEST_CASE(stress_AtomicQueue2) {
    stress<RetryDecorator<AtomicQueue2<unsigned, CAPACITY>>>();
}

BOOST_AUTO_TEST_CASE(stress_BlockingAtomicQueue2) {
    stress<AtomicQueue2<unsigned, CAPACITY>>();
}

BOOST_AUTO_TEST_CASE(move_only_2) {
    AtomicQueue2<std::unique_ptr<int>, 2> q;
    test_unique_ptr_int(q);
}

BOOST_AUTO_TEST_CASE(move_only_b2) {
    AtomicQueueB2<std::unique_ptr<int>> q(2);
    test_unique_ptr_int(q);
}

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

BOOST_AUTO_TEST_CASE(move_constructor_assignment) {
    AtomicQueueB2<std::unique_ptr<int>> q(2);
    auto q2 = std::move(q);
    q = std::move(q2);

    AtomicQueueB<int> p(2);
    auto p2 = std::move(p);
    p = std::move(p2);
}

BOOST_AUTO_TEST_CASE(try_push) {
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
    BOOST_CHECK_EQUAL(q.was_size(), 0u);

    for(unsigned i = 1; i <= CAPACITY; ++i)
        BOOST_CHECK(q.try_push(i));

    BOOST_CHECK(!q.was_empty());
    BOOST_CHECK_EQUAL(q.was_size(), CAPACITY);

    for(unsigned i = 1; i <= CAPACITY; ++i)
        BOOST_CHECK(!q.try_push(i));

    BOOST_CHECK(!q.was_empty());
    BOOST_CHECK_EQUAL(q.was_size(), CAPACITY);
}

BOOST_AUTO_TEST_CASE(size) {
    atomic_queue::RetryDecorator<atomic_queue::AtomicQueueB2<float>> q(10);
    BOOST_CHECK_EQUAL(q.capacity(), CACHE_LINE_SIZE * CACHE_LINE_SIZE);
}

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
