/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED
#define ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED

#include <cassert>
#include <utility>
#include <atomic>

#include <emmintrin.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

int constexpr CACHE_LINE_SIZE = 64;

auto constexpr A = std::memory_order_acquire;
auto constexpr R = std::memory_order_release;
auto constexpr X = std::memory_order_relaxed;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Derived>
class AtomicQueueCommon {
protected:
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> head_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> tail_ = {};

public:
    template<class T>
    bool try_push(T&& element) {
        auto head = head_.load(A);
        do {
            if(head - tail_.load(X) >= Derived::size)
                return false;
        } while(!head_.compare_exchange_strong(head, head + 1, X, X));

        static_cast<Derived&>(*this).do_push(std::forward<T>(element), head);
        return true;
    }

    template<class T>
    bool try_pop(T& element) {
        auto tail = tail_.load(A);
        do {
            if(head_.load(X) == tail)
                return false;
        } while(!tail_.compare_exchange_strong(tail, tail + 1, X, X));

        element = static_cast<Derived&>(*this).do_pop(tail);
        return true;
    }

    template<class T>
    void push(T&& element) {
        auto head = head_.fetch_add(1, A);
        static_cast<Derived&>(*this).do_push(std::forward<T>(element), head);
    }

    auto pop() {
        auto tail = tail_.fetch_add(1, A);
        return static_cast<Derived&>(*this).do_pop(tail);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE, T NIL = T{}>
class AtomicQueue : public AtomicQueueCommon<AtomicQueue<T, SIZE, NIL>> {
    alignas(CACHE_LINE_SIZE) std::atomic<T> q_[SIZE] = {}; // Empty elements are NIL.

    friend class AtomicQueueCommon<AtomicQueue<T, SIZE, NIL>>;

    static constexpr auto size = SIZE;

    T do_pop(unsigned tail) {
        unsigned index = tail % SIZE;
        for(;;) {
            T element = q_[index].load(X);
            if(element != NIL) {
                q_[index].store(NIL, R); // (2) Mark the element as empty.
                return element;
            }
            _mm_pause();
        }
    }

    void do_push(T element, unsigned head) {
        assert(element != NIL);
        unsigned index = head % SIZE;
        for(T expected = NIL; !q_[index].compare_exchange_strong(expected, element, R, X); expected = NIL) // (1) Wait for store (2) to complete.
            _mm_pause();
    }

public:
    using value_type = T;

    AtomicQueue() {
        assert(std::atomic<T>{NIL}.is_lock_free());
        if(T{} != NIL)
            for(auto& element : q_)
                element.store(NIL, X);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE>
class AtomicQueue2 : public AtomicQueueCommon<AtomicQueue2<T, SIZE>> {
    using Unit = unsigned;
    static int constexpr BITS_PER_UNIT = sizeof(Unit) * 8;
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> e_[(SIZE + BITS_PER_UNIT - 1) / BITS_PER_UNIT] = {}; // TODO: Reduce contention on the bits.
    alignas(CACHE_LINE_SIZE) T q_[SIZE] = {};

    friend class AtomicQueueCommon<AtomicQueue2<T, SIZE>>;

    static constexpr auto size = SIZE;

    T do_pop(unsigned tail) {
        auto index = tail % SIZE;
        auto e_index = index / BITS_PER_UNIT;
        auto bit = Unit{1} << (index % BITS_PER_UNIT);
        while(!(e_[e_index].load(std::memory_order_relaxed) & bit)) // Spin-wait for mark element occupied in (1) to complete.
            _mm_pause();
        T element{std::move(q_[index])};
        e_[e_index].fetch_xor(bit, std::memory_order_release); // (2) Mark the element as empty.
        return element;
    }

    template<class U>
    void do_push(U&& element, unsigned head) {
        auto index = head % SIZE;
        auto e_index = index / BITS_PER_UNIT;
        auto bit = Unit{1} << (index % BITS_PER_UNIT);
        while(e_[e_index].load(std::memory_order_relaxed) & bit) // Spin-wait for mark element empty in (2) to complete.
            _mm_pause();
        q_[index] = std::forward<U>(element);
        e_[e_index].fetch_or(bit, std::memory_order_release); // (1) Mark the element as occupied.
    }

public:
    using value_type = T;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
struct RetryDecorator : Queue {
    using T = typename Queue::value_type;

    using Queue::Queue;

    void push(T element) {
        while(!this->try_push(element))
            _mm_pause();
    }

    T pop() {
        T element;
        while(!this->try_pop(element))
            _mm_pause();
        return element;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED
