/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE atomic_queue
#include <boost/test/unit_test.hpp>

#include "atomic_queue_spin_lock.h"
#include "atomic_queue.h"
#include "barrier.h"

#include <numeric>
#include <cstdint>
#include <thread>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace ::atomic_queue;

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Check that all push'es are ever pop'ed once with multiple producer and multiple consumers.
template<class Queue>
void stress() {
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

    uint64_t result = std::accumulate(results, results + CONSUMERS, uint64_t{});
    constexpr uint64_t expected_result = (N + 1) / 2. * N;
    int64_t result_diff = result / CONSUMERS - expected_result;
    BOOST_CHECK_EQUAL(result_diff, 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr unsigned CAPACITY = 1024;

BOOST_AUTO_TEST_CASE(stress_AtomicQueue) {
    stress<RetryDecorator<AtomicQueue <unsigned, CAPACITY>>>();
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

BOOST_AUTO_TEST_CASE(stress_pthread_spinlock) {
    stress<RetryDecorator<AtomicQueueSpinLock<unsigned, CAPACITY>>>();
}

BOOST_AUTO_TEST_CASE(stress_SpinLockHle) {
    stress<RetryDecorator<AtomicQueueSpinLockHle<unsigned, CAPACITY>>>();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
