/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "atomic_queue/atomic_queue.h"

#include <thread>
#include <iostream>

int main() {
    int constexpr N_PRODUCERS = 1; // Number of producer threads.
    int constexpr N_CONSUMERS = 2; // Number of consumer threads.
    enum { N_PRODUCER_MSG = 1'000'000 }; // Each producer pushes this many elements into the queue.
    unsigned constexpr CAPACITY = 1024;  // Queue capacity. Since there are more consumers than producers the queue doesn't need to be large.

    using Element = unsigned;   // Queue element type.
    Element constexpr NIL = -1; // Atomic elements require a special value that cannot be pushed/popped.
    using Queue = atomic_queue::AtomicQueueB<Element, std::allocator<Element>, NIL>; // A queue with a heap-allocated buffer.

    // Create a queue object shared between producer and consumer threads.
    Queue q{CAPACITY};

    // Start the consumer threads.
    uint64_t sums[N_CONSUMERS];
    std::thread consumers[N_CONSUMERS];
    for(auto& consumer : consumers)
        consumer = std::thread([&q, &sum = sums[&consumer - consumers]]() {
            uint64_t s = 0; // New object with automatic storage duration. Not aliased or false-shared by construction.
            while(Element n = q.pop()) // Break the loop when 0 is pop'ed.
                s += n;
            // Store into sum only once because it is element of sums array, false-sharing the same cache line with other threads.
            // Updating sum in the loop above saturates the inter-core bus with cache coherence protocol messages.
            sum = s;
        });

    // Start the producer threads.
    std::thread producers[N_PRODUCERS];
    for(auto& producer : producers)
        producer = std::thread([&q]() {
            // Each producer pushes range [1, N] elements into the queue.
            // Ascending order [1, N] requires comparing with N at each loop iteration. Ascending order isn't necessary here.
            // Push elements in descending order, range [N, 1] with step -1, so that CPU decrement instruction sets zero/equal flag
            // when 0 is reached, which breaks the loop without having to compare n with N at each iteration.
            for(Element n = N_PRODUCER_MSG; n; --n)
                q.push(n);
        });

    // Wait till producers have terminated.
    for(auto& producer : producers)
        producer.join();

    // Tell consumers to terminate by pushing one 0 element for each consumer.
    for([[maybe_unused]] auto& consumer : consumers)
        q.push(0);
    // Wait till consumers have terminated.
    for(auto& consumer : consumers)
        consumer.join();
    // When all consumers have terminated the queue is empty.

    // Sum up consumer's received elements sums.
    uint64_t total_sum = 0;
    for(auto& sum : sums) {
        total_sum += sum;
        if(!sum) // Verify that each consumer received at least one element.
            std::cerr << "WARNING: consumer " << (&sum - sums) << " received no elements.\n";
    }

    // Verify that each element has been pop'ed exactly once; not corrupted, dropped or duplicated.
    uint64_t constexpr expected_total_sum = (N_PRODUCER_MSG + 1) * .5 * N_PRODUCER_MSG * N_PRODUCERS;
    if(int64_t total_sum_diff = total_sum - expected_total_sum) {
        std::cerr << "ERROR: unexpected total_sum difference " << total_sum_diff << '\n';
        return EXIT_FAILURE;
    }
}
