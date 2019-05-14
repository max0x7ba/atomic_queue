/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE atomic_queue
#include <boost/test/unit_test.hpp>

#include "atomic_queue_spin_lock.h"
#include "atomic_queue.h"
#include "barrier.h"

#include <numeric>
#include <thread>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace ::atomic_queue;

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Check that all push'es are ever pop'ed once with multiple producer and multiple consumers.
template<class Queue>
void test_correctness() {
    constexpr int PRODUCERS = 2;
    constexpr int CONSUMERS = 2;
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
    uint64_t result = std::accumulate(results, results + CONSUMERS, uint64_t{});
    BOOST_CHECK_EQUAL(expected_result, result / CONSUMERS);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr unsigned CAPACITY = 1000;

BOOST_AUTO_TEST_CASE(correctness_AtomicQueue) {
    test_correctness<RetryDecorator<AtomicQueue <unsigned, CAPACITY>>>();
}

BOOST_AUTO_TEST_CASE(correctness_BlockingAtomicQueue) {
    test_correctness<AtomicQueue<unsigned, CAPACITY>>();
}

BOOST_AUTO_TEST_CASE(correctness_AtomicQueue2) {
    test_correctness<RetryDecorator<AtomicQueue2<unsigned, CAPACITY>>>();
}

BOOST_AUTO_TEST_CASE(correctness_BlockingAtomicQueue2) {
    test_correctness<AtomicQueue2<unsigned, CAPACITY>>();
}

BOOST_AUTO_TEST_CASE(correctness_pthread_spinlock) {
    test_correctness<RetryDecorator<AtomicQueueSpinLock<unsigned, CAPACITY>>>();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
