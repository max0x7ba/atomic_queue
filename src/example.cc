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
    unsigned constexpr CAPACITY = 1024; // Queue capacity. Since there are more consumers than producers the queue doesn't need to be large.

    using Element = uint32_t; // Queue element type.
    Element constexpr NIL = static_cast<Element>(-1); // Atomic elements require a special value that cannot be pushed/popped.
    using Queue = atomic_queue::AtomicQueueB<Element, std::allocator<Element>, NIL>; // Use heap-allocated buffer.

    // Create a queue object shared between all producers and consumers.
    Queue q{CAPACITY};

    // Start the consumers.
    uint64_t sums[CONSUMERS];
    std::thread consumers[CONSUMERS];
    for(int i = 0; i < CONSUMERS; ++i)
        consumers[i] = std::thread([&q, &sum = sums[i]]() {
            uint64_t s = 0; // New object with automatic storage duration. Not aliased or false-shared by construction.
            while(Element n = q.pop()) // Break the loop when 0 is pop'ed.
                s += n;
            // Store into sum only once because it is element of sums array, false-sharing the same cache line with other threads.
            // Updating sum in the loop above saturates the inter-core bus with cache coherence protocol messages.
            sum = s;
        });

    // Start the producers.
    std::thread producers[PRODUCERS];
    for(int i = 0; i < PRODUCERS; ++i)
        producers[i] = std::thread([&q]() {
            // Each producer pushes range [1, N] elements into the queue.
            // Ascending order [1, N] requires comparing with N at each loop iteration. Ascending order isn't necessary here.
            // Push elements in descending order, range [N, 1] with step -1, so that CPU decrement instruction sets zero/equal flag
            // when 0 is reached, which breaks the loop without having to compare n with N at each iteration.
            for(Element n = N; n; --n)
                q.push(n);
        });

    // Wait till producers have terminated.
    for(auto& t : producers)
        t.join();

    // Tell consumers to terminate by pushing one 0 element for each consumer.
    for(int i = CONSUMERS; i--;)
        q.push(0);
    // Wait till consumers have terminated.
    for(auto& t : consumers)
        t.join();
    // When all consumers have terminated the queue is empty.

    // Sum up consumer's received elements sums.
    uint64_t total_sum = 0;
    for(auto& sum : sums) {
        total_sum += sum;
        if(!sum) // Verify that each consumer received at least one element.
            std::cerr << "WARNING: consumer " << (&sum - sums) << " received no elements.\n";
    }

    // Verify that each element has been pop'ed exactly once; not corrupted, dropped or duplicated.
    uint64_t constexpr expected_total_sum = (N + 1) / 2. * N * PRODUCERS;
    if(int64_t total_sum_diff = total_sum - expected_total_sum) {
        std::cerr << "ERROR: unexpected total_sum difference " << total_sum_diff << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
