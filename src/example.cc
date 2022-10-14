/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "atomic_queue/atomic_queue.h"

#include <thread>
#include <cstdint>
#include <iostream>

int main() {
    int constexpr PRODUCERS = 1; // Number of producer threads.
    int constexpr CONSUMERS = 2; // Number of consumer threads.
    unsigned constexpr N = 1000000; // Each producer pushes this many elements into the queue.
    unsigned constexpr CAPACITY = 1024; // Queue capacity. Since there are more consumers than producers this doesn't have to be large.

    using Element = uint32_t; // Queue element type.
    Element constexpr NIL = static_cast<Element>(-1); // Atomic elements require a special value that cannot be pushed/popped.
    using Queue = atomic_queue::AtomicQueueB<Element, std::allocator<Element>, NIL>; // Use heap-allocated buffer.

    // Create a queue object shared between all producers and consumers.
    Queue q{CAPACITY};

    // Start the consumers.
    uint64_t results[CONSUMERS];
    std::thread consumers[CONSUMERS];
    for(int i = 0; i < CONSUMERS; ++i)
        consumers[i] = std::thread([&q, &r = results[i]]() {
            uint64_t sum = 0; // Not aliased with anything else or false-shared with other threads by construction in thread's stack.
            while(Element n = q.pop()) // Break the loop when 0 is pop'ed.
                sum += n;
            r = sum; // r is an element of results array, which is false-shared in the same cache line with other consumers.
        });

    // Start the producers.
    std::thread producers[PRODUCERS];
    for(int i = 0; i < PRODUCERS; ++i)
        producers[i] = std::thread([&q]() {
            // Each producer pushes range [1, N] elements into the queue.
            // Ascending order [1, N] requires comparing with N at each loop iteration. Ascending order isn't necessary here.
            // Push elements in descending order, range [N, 1] with step -1, so that CPU decrement instruction sets zero/equal flag
            // naturally when 0 is reached which breaks the loop without having to compare n with N at each iteration.
            for(Element n = N; n; --n)
                q.push(n);
        });

    // Wait till producers complete and terminate.
    for(auto& t : producers)
        t.join();

    // Tell each consumer to break the loop and terminate by pushing one 0 element for each consumer.
    // Each consumer terminates upon pop'ing exactly one 0 element.
    for(int i = CONSUMERS; i--;)
        q.push(0);
    // Wait till consumers complete and terminate.
    for(auto& t : consumers)
        t.join();
    // When all consumers have terminated the queue has been drained empty and is ready for use as new.

    // Verify that each consumer received at least one element.
    // Reduce consumers' received elements sums into total sum.
    uint64_t result = 0;
    for(auto& r : results) {
        result += r;
        if(!r)
            std::cerr << "WARNING: consumer " << (&r - results) << " received no elements.\n";
    }

    // Verify that each element has been pop'ed exactly once; not corrupted, dropped or duplicated.
    uint64_t constexpr expected_result = (N + 1) / 2. * N * PRODUCERS;
    if(int64_t result_diff = result - expected_result) {
        std::cerr << "ERROR: unexpected result difference " << result_diff << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
