/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_H_INCLUDED
#define ATOMIC_QUEUE_H_INCLUDED

#include <boost/atomic/detail/pause.hpp>

#include <cassert>
#include <utility>
#include <atomic>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE, T NIL = T{}>
class AtomicQueue {
    static int constexpr CACHE_LINE_SIZE = 64;
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> head_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> tail_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<T> q_[SIZE] = {}; // Empty elements are NIL.

    static auto constexpr A = std::memory_order_acquire;
    static auto constexpr R = std::memory_order_release;
    static auto constexpr X = std::memory_order_relaxed;

public:
    AtomicQueue() {
        assert(q_[0].is_lock_free());
        if(q_[0] != NIL)
            for(auto& element : q_)
                element.store(NIL, X);
    }

    bool try_push(T element) {
        assert(element != NIL);
        auto head = head_.load(A);
        if(head - tail_.load(X) < SIZE && head_.compare_exchange_strong(head, head + 1, X, X)) {
            auto& q_element = q_[head % SIZE];
            T expected;
            do expected = NIL;
            while(!q_element.compare_exchange_weak(expected, element, R, X)); // (1) Wait for store (2) to complete.
            return true;
        }
        return false;
    }

    bool try_pop(T& element) {
        auto tail = tail_.load(A);
        if(head_.load(X) != tail && tail_.compare_exchange_strong(tail, tail + 1, X, X)) {
            auto& q_element = q_[tail % SIZE];
            do element = q_element.exchange(NIL, R); // (2) Wait for store (1) to complete.
            while(element == NIL);
            return true;
        }
        return false;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE>
class AtomicQueue2 {
    using Unit = unsigned;
    static int constexpr BITS_PER_UNIT = sizeof(Unit) * 8;
    static int constexpr CACHE_LINE_SIZE = 64;
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> head_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> tail_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> u_[(SIZE + BITS_PER_UNIT - 1) / BITS_PER_UNIT] = {};
    alignas(CACHE_LINE_SIZE) T q_[SIZE] = {};

public:
    template<class U>
    bool try_push(U&& element) {
        auto head = head_.fetch_add(1, std::memory_order_acquire); // Grab the next index unconditionally.
        if(head - tail_.load(std::memory_order_relaxed) < SIZE) {
            auto index = head % SIZE;
            auto unit_index = index / BITS_PER_UNIT;
            auto bit = Unit{1} << (index % BITS_PER_UNIT);
            while(u_[unit_index].load(std::memory_order_relaxed) & bit) // Spin-wait for mark element empty in (2) to complete.
                ::boost::atomics::detail::pause();
            q_[index] = std::forward<U>(element);
            u_[unit_index].fetch_or(bit, std::memory_order_release); // (1) Mark the element as occupied.
            return true;
        }
        head_.fetch_sub(1, std::memory_order_release); // The queue is/was full. Undo the index grab.
        return false;
    }

    bool try_pop(T& element) {
        auto tail = tail_.fetch_add(1, std::memory_order_acquire); // Grab the next index unconditionally.
        if(static_cast<int>(head_.load(std::memory_order_relaxed) - tail) > 0) {
            auto index = tail % SIZE;
            auto unit_index = index / BITS_PER_UNIT;
            auto bit = Unit{1} << (index % BITS_PER_UNIT);
            while(!(u_[unit_index].load(std::memory_order_relaxed) & bit)) // Spin-wait for mark element occupied in (1) to complete.
                ::boost::atomics::detail::pause();
            element = std::move(q_[index]);
            u_[unit_index].fetch_xor(bit, std::memory_order_release); // (2) Mark the element as empty.
            return true;
        }
        tail_.fetch_sub(1, std::memory_order_release); // The queue is/was empty. Undo the index grab.
        return false;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // QUORUM_ATOMIC_QUEUE_H_INCLUDED
