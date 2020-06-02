/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE atomic_queue
#include <boost/test/unit_test.hpp>

#include "atomic_queue.h"
#include "atomic_queue_mutex.h"
#include "barrier.h"

#include <cstdint>
#include <thread>

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
        producers[i] = std::thread([&q, &barrier]() {
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
        BOOST_CHECK_GT(r, expected_result / (CONSUMERS + 1)); // Make sure a consumer didn't starve. False positives are possible here.
        result += r;
    }

    int64_t result_diff = result / CONSUMERS - expected_result;
    BOOST_CHECK_EQUAL(result_diff, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Q>
void test_unique_ptr_int(Q& q) {
    BOOST_CHECK(q.was_empty());
    BOOST_CHECK_EQUAL(q.was_size(), 0);
    std::unique_ptr<int> p{new int{1}};
    BOOST_REQUIRE(q.try_push(move(p)));
    BOOST_CHECK(!p);
    BOOST_CHECK(!q.was_empty());
    BOOST_CHECK_EQUAL(q.was_size(), 1);

    p.reset(new int{2});
    q.push(move(p));
    BOOST_REQUIRE(!p);
    BOOST_CHECK(!q.was_empty());
    BOOST_CHECK_EQUAL(q.was_size(), 2);

    BOOST_REQUIRE(q.try_pop(p));
    BOOST_REQUIRE(p.get());
    BOOST_CHECK_EQUAL(*p, 1);
    BOOST_CHECK(!q.was_empty());
    BOOST_CHECK_EQUAL(q.was_size(), 1);

    p = q.pop();
    BOOST_REQUIRE(p.get());
    BOOST_CHECK_EQUAL(*p, 2);
    BOOST_CHECK(q.was_empty());
    BOOST_CHECK_EQUAL(q.was_size(), 0);
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

BOOST_AUTO_TEST_CASE(move_constructor_assignment) {
    AtomicQueueB2<std::unique_ptr<int>> q(2);
    auto q2 = std::move(q);
    q = std::move(q2);

    AtomicQueueB<int> p(2);
    auto p2 = std::move(p);
    p = std::move(p2);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
