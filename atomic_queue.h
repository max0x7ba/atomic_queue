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
auto constexpr C = std::memory_order_seq_cst;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Derived>
class AtomicQueueCommon {
protected:
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> head_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> tail_ = {};

public:
    template<class T>
    bool try_push(T&& element) {
        auto head = head_.load(X);
        do {
            if(static_cast<int>(head - tail_.load(X)) >= static_cast<int>(Derived::size))
                return false;
        } while(!head_.compare_exchange_weak(head, head + 1, A, X));

        static_cast<Derived&>(*this).do_push(std::forward<T>(element), head);
        return true;
    }

    template<class T>
    bool try_pop(T& element) {
        auto tail = tail_.load(X);
        do {
            if(static_cast<int>(head_.load(X) - tail) <= 0)
                return false;
        } while(!tail_.compare_exchange_weak(tail, tail + 1, A, X));

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
    alignas(CACHE_LINE_SIZE) std::atomic<T> elements_[SIZE] = {}; // Empty elements are NIL.

    friend class AtomicQueueCommon<AtomicQueue<T, SIZE, NIL>>;

    static constexpr auto size = SIZE;

    T do_pop(unsigned tail) {
        unsigned index = tail % SIZE;
        for(;;) {
            T element = elements_[index].exchange(NIL, R);
            if(element != NIL)
                return element;
            _mm_pause();
        }
    }

    void do_push(T element, unsigned head) {
        assert(element != NIL);
        unsigned index = head % SIZE;
        for(T expected = NIL; !elements_[index].compare_exchange_weak(expected, element, R, X); expected = NIL) // (1) Wait for store (2) to complete.
            _mm_pause();
    }

public:
    using value_type = T;

    AtomicQueue() {
        assert(std::atomic<T>{NIL}.is_lock_free());
        if(T{} != NIL)
            for(auto& element : elements_)
                element.store(NIL, X);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE>
class AtomicQueue2 : public AtomicQueueCommon<AtomicQueue2<T, SIZE>> {
    enum State : unsigned char {
        EMPTY,
        STORING,
        STORED,
        LOADING
    };
    std::atomic<unsigned char> states_[SIZE] = {};
    alignas(CACHE_LINE_SIZE) T elements_[SIZE];

    friend class AtomicQueueCommon<AtomicQueue2<T, SIZE>>;

    static constexpr auto size = SIZE;

    T do_pop(unsigned tail) {
        auto index = tail % SIZE;
        for(;;) {
            unsigned char expected = STORED;
            if(states_[index].compare_exchange_weak(expected, LOADING, X, X)) {
                T element{std::move(elements_[index])};
                states_[index].store(EMPTY, R);
                return element;
            }
            _mm_pause();
        }
    }

    template<class U>
    void do_push(U&& element, unsigned head) {
        auto index = head % SIZE;
        for(;;) {
            unsigned char expected = EMPTY;
            if(states_[index].compare_exchange_weak(expected, STORING, X, X)) {
                elements_[index] = std::forward<U>(element);
                states_[index].store(STORED, R);
                return;
            }
            _mm_pause();
        }
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
