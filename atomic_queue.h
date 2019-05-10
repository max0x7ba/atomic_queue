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

template<class T, unsigned SIZE, T NIL = T{}>
class AtomicQueue {
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> head_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> tail_ = {}; // Invariant: (head_ - tail_) >= 0.
    alignas(CACHE_LINE_SIZE) std::atomic<T> q_[SIZE] = {}; // Empty elements are NIL.

public:
    AtomicQueue() {
        assert(std::atomic<T>{NIL}.is_lock_free());
        if(T{} != NIL)
            for(auto& element : q_)
                element.store(NIL, X);
    }

    bool try_push(T element) {
        assert(element != NIL);

        unsigned head;
        do {
            head = head_.load(A);
            if(head - tail_.load(X) >= SIZE)
                return false;
        } while(!head_.compare_exchange_strong(head, head + 1, X, X));

        auto& q_element = q_[head % SIZE];
        T expected;
        do expected = NIL;
        while(!q_element.compare_exchange_weak(expected, element, R, X)); // (1) Wait for store (2) to complete.
        return true;
    }

    bool try_pop(T& element) {
        unsigned tail;
        do {
            tail = tail_.load(A);
            if(head_.load(X) == tail)
                return false;
        } while(!tail_.compare_exchange_strong(tail, tail + 1, X, X));

        auto& q_element = q_[tail % SIZE];
        do element = q_element.exchange(NIL, R); // (2) Wait for store (1) to complete.
        while(element == NIL);
        return true;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, class Base>
struct TryDecorator : Base {
    using Base::Base;

    bool empty() const {
        return this->head_.load(X) == this->tail_.load(X); // head_ may have increased during or after this expression evaluated.
    }

    bool full() const {
        return this->head_.load(X) - this->tail_.load(X) >= this->capacity(); // head_ or tail_ may have increased during or after this expression evaluated.
    }

    bool try_push(T element) {
        // if(this->full())
        //     return false;
        this->push(element);
        return true;
    }

    bool try_pop(T& element) {
        if(this->empty())
            return false;
        element = this->pop();
        return true;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE, T NIL = T{}>
class BlockingAtomicQueue_ {
protected:
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> head_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> tail_ = {}; // Invariant: (head_ - tail_) >= 0.
    alignas(CACHE_LINE_SIZE) std::atomic<T> q_[SIZE] = {}; // Empty elements are NIL.

public:
    BlockingAtomicQueue_() {
        assert(std::atomic<T>{NIL}.is_lock_free());
        if(T{} != NIL)
            for(auto& element : q_)
                element.store(NIL, X);
    }

    void push(T element) {
        assert(element != NIL);

        unsigned i = head_.fetch_add(1, A) % SIZE;
        for(T expected = NIL; !q_[i].compare_exchange_strong(expected, element, R, X); expected = NIL) // (1) Wait for store (2) to complete.
            _mm_pause();
    }

    T pop() {
        unsigned i = tail_.fetch_add(1, A) % SIZE;
        for(;;) {
            T element = q_[i].load(X);
            if(element != NIL) {
                q_[i].store(NIL, R); // (2) Mark the element as empty.
                return element;
            }
            _mm_pause();
        }
    }

    static unsigned capacity() { return SIZE; }
};

template<class T, unsigned SIZE, T NIL = T{}>
using BlockingAtomicQueue = TryDecorator<T, BlockingAtomicQueue_<T, SIZE, NIL>>;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE>
class AtomicQueue2 {
    using Unit = unsigned;
    static int constexpr BITS_PER_UNIT = sizeof(Unit) * 8;
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> head_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> tail_ = {}; // Invariant: (head_ - tail_) >= 0.
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> e_[(SIZE + BITS_PER_UNIT - 1) / BITS_PER_UNIT] = {};
    alignas(CACHE_LINE_SIZE) T q_[SIZE] = {};

public:
    template<class U>
    bool try_push(U&& element) {
        auto head = head_.load(A);
        if(head - tail_.load(X) < SIZE && head_.compare_exchange_strong(head, head + 1, X, X)) {
            auto index = head % SIZE;
            auto e_index = index / BITS_PER_UNIT;
            auto bit = Unit{1} << (index % BITS_PER_UNIT);
            while(e_[e_index].load(std::memory_order_relaxed) & bit) // Spin-wait for mark element empty in (2) to complete.
                _mm_pause();
            q_[index] = std::forward<U>(element);
            e_[e_index].fetch_or(bit, std::memory_order_release); // (1) Mark the element as occupied.
            return true;
        }
        return false;
    }

    bool try_pop(T& element) {
        auto tail = tail_.load(A);
        if(head_.load(X) != tail && tail_.compare_exchange_strong(tail, tail + 1, X, X)) {
            auto index = tail % SIZE;
            auto e_index = index / BITS_PER_UNIT;
            auto bit = Unit{1} << (index % BITS_PER_UNIT);
            while(!(e_[e_index].load(std::memory_order_relaxed) & bit)) // Spin-wait for mark element occupied in (1) to complete.
                _mm_pause();
            element = std::move(q_[index]);
            e_[e_index].fetch_xor(bit, std::memory_order_release); // (2) Mark the element as empty.
            return true;
        }
        return false;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE>
class BlockingAtomicQueue2 {
    using Unit = unsigned;
    static int constexpr BITS_PER_UNIT = sizeof(Unit) * 8;
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> head_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> tail_ = {}; // Invariant: (head_ - tail_) >= 0.
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> e_[(SIZE + BITS_PER_UNIT - 1) / BITS_PER_UNIT] = {};
    alignas(CACHE_LINE_SIZE) std::atomic<T> q_[SIZE] = {}; // Empty elements are NIL.

public:
    template<class U>
    void push(U&& element) {
        auto index = head_.fetch_add(1, A) % SIZE;
        auto e_index = index / BITS_PER_UNIT;
        auto bit = Unit{1} << (index % BITS_PER_UNIT);
        while(e_[e_index].load(std::memory_order_relaxed) & bit) // Spin-wait for mark element empty in (2) to complete.
            _mm_pause();
        q_[index] = std::forward<U>(element);
        e_[e_index].fetch_or(bit, std::memory_order_release); // (1) Mark the element as occupied.
    }

    T pop() {
        auto index = tail_.fetch_add(1, A) % SIZE;
        auto e_index = index / BITS_PER_UNIT;
        auto bit = Unit{1} << (index % BITS_PER_UNIT);
        while(!(e_[e_index].load(std::memory_order_relaxed) & bit)) // Spin-wait for mark element occupied in (1) to complete.
            _mm_pause();
        T element{std::move(q_[index])};
        e_[e_index].fetch_xor(bit, std::memory_order_release); // (2) Mark the element as empty.
        return element;
    }

    bool empty() const {
        return head_.load(X) == tail_.load(X); // head_ may have increased during or after this expression evaluated.
    }

    template<class U>
    bool try_push(U&& element) {
        this->push(std::forward<U>(element));
        return true;
    }

    bool try_pop(T& element) {
        element = this->pop();
        return true;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED
