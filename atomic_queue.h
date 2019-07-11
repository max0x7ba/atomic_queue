/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED
#define ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "defs.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

using std::uint32_t;
using std::uint64_t;
using std::uint8_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace details {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<size_t elements_per_cache_line>
struct GetCacheLineIndexBits {
    static int constexpr value = 0;
};
template<>
struct GetCacheLineIndexBits<64> {
    static int constexpr value = 6;
};
template<>
struct GetCacheLineIndexBits<32> {
    static int constexpr value = 5;
};
template<>
struct GetCacheLineIndexBits<16> {
    static int constexpr value = 4;
};
template<>
struct GetCacheLineIndexBits<8> {
    static int constexpr value = 3;
};
template<>
struct GetCacheLineIndexBits<4> {
    static int constexpr value = 2;
};
template<>
struct GetCacheLineIndexBits<2> {
    static int constexpr value = 1;
};

template<bool minimize_contention, unsigned array_size, size_t elements_per_cache_line>
struct GetIndexShuffleBits {
    static int constexpr bits = GetCacheLineIndexBits<elements_per_cache_line>::value;
    static unsigned constexpr min_size = 1u << (bits * 2);
    static int constexpr value = array_size < min_size ? 0 : bits;
};

template<unsigned array_size, size_t elements_per_cache_line>
struct GetIndexShuffleBits<false, array_size, elements_per_cache_line> {
    static int constexpr value = 0;
};

// Multiple writers/readers contend on the same cache line when storing/loading elements at
// subsequent indexes, aka false sharing. For power of 2 ring buffer size it is possible to re-map
// the index in such a way that each subsequent element resides on another cache line, which
// minimizes contention. This is done by swapping the lowest order N bits (which are the index of
// the element within the cache line) with the next N bits (which are the index of the cache line)
// of the element index.
template<int BITS>
constexpr unsigned remap_index(unsigned index) noexcept {
    constexpr unsigned MASK = (1u << BITS) - 1;
    unsigned part0 = (index >> BITS) & MASK;
    unsigned part1 = (index & MASK) << BITS;
    unsigned part2 = index & ~(MASK | MASK << BITS);
    return part0 | part1 | part2;
}

template<>
constexpr unsigned remap_index<0>(unsigned index) noexcept {
    return index;
}

template<int BITS, class T>
constexpr T& map(T* elements, unsigned index) noexcept {
    index = remap_index<BITS>(index);
    return elements[index];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

constexpr uint32_t round_up_to_power_of_2(uint32_t a) noexcept {
    --a;
    a |= a >> 1;
    a |= a >> 2;
    a |= a >> 4;
    a |= a >> 8;
    a |= a >> 16;
    ++a;
    return a;
}

constexpr uint64_t round_up_to_power_of_2(uint64_t a) noexcept {
    --a;
    a |= a >> 1;
    a |= a >> 2;
    a |= a >> 4;
    a |= a >> 8;
    a |= a >> 16;
    a |= a >> 32;
    ++a;
    return a;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace details

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Derived>
class AtomicQueueCommon {
protected:
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> head_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> tail_ = {};

    AtomicQueueCommon() noexcept = default;

    AtomicQueueCommon(AtomicQueueCommon const& b) noexcept
        : head_(b.head_.load(X))
        , tail_(b.tail_.load(X)) {}

    AtomicQueueCommon& operator=(AtomicQueueCommon const& b) noexcept {
        head_.store(b.head_.load(X), X);
        tail_.store(b.tail_.load(X), X);
        return *this;
    }

    void swap(AtomicQueueCommon& b) noexcept {
        unsigned h = head_.load(X);
        unsigned t = tail_.load(X);
        head_.store(b.head_.load(X), X);
        tail_.store(b.tail_.load(X), X);
        b.head_.store(h, X);
        b.tail_.store(t, X);
    }

public:
    template<class T>
    bool try_push(T&& element) noexcept {
        auto head = head_.load(X);
        do {
            if(static_cast<int>(head - tail_.load(X)) >= static_cast<int>(static_cast<Derived&>(*this).size_))
                return false;
        } while(!head_.compare_exchange_strong(head, head + 1, A, X)); // This loop is not FIFO.

        static_cast<Derived&>(*this).do_push(std::forward<T>(element), head);
        return true;
    }

    template<class T>
    bool try_pop(T& element) noexcept {
        auto tail = tail_.load(X);
        do {
            if(static_cast<int>(head_.load(X) - tail) <= 0)
                return false;
        } while(!tail_.compare_exchange_strong(tail, tail + 1, A, X)); // This loop is not FIFO.

        element = static_cast<Derived&>(*this).do_pop(tail);
        return true;
    }

    template<class T>
    void push(T&& element) noexcept {
        constexpr auto memory_order = Derived::total_order_ ? std::memory_order_seq_cst : std::memory_order_acquire;
        auto head = head_.fetch_add(1, memory_order); // FIFO and total order on Intel regardless, as of 2019.
        static_cast<Derived&>(*this).do_push(std::forward<T>(element), head);
    }

    auto pop() noexcept {
        constexpr auto memory_order = Derived::total_order_ ? std::memory_order_seq_cst : std::memory_order_acquire;
        auto tail = tail_.fetch_add(1, memory_order); // FIFO and total order on Intel regardless, as of 2019.
        return static_cast<Derived&>(*this).do_pop(tail);
    }

    bool was_empty() const noexcept {
        return static_cast<int>(head_.load(X) - tail_.load(X)) <= 0;
    }

    bool was_full() const noexcept {
        return static_cast<int>(head_.load(X) - tail_.load(X)) >= static_cast<int>(static_cast<Derived&>(*this).size_);
    }

    unsigned size() const noexcept {
        return static_cast<Derived&>(*this).size_;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE, T NIL = T{}, bool MINIMIZE_CONTENTION = true, bool TOTAL_ORDER = false>
class AtomicQueue : public AtomicQueueCommon<AtomicQueue<T, SIZE, NIL, MINIMIZE_CONTENTION, TOTAL_ORDER>> {
    using Base = AtomicQueueCommon<AtomicQueue<T, SIZE, NIL, MINIMIZE_CONTENTION, TOTAL_ORDER>>;
    friend Base;

    static constexpr unsigned size_ = MINIMIZE_CONTENTION ? details::round_up_to_power_of_2(SIZE) : SIZE;
    static constexpr int SHUFFLE_BITS =
        details::GetIndexShuffleBits<MINIMIZE_CONTENTION, size_, CACHE_LINE_SIZE / sizeof(std::atomic<T>)>::value;
    static constexpr bool total_order_ = TOTAL_ORDER;

    alignas(CACHE_LINE_SIZE) std::atomic<T> elements_[size_] = {}; // Empty elements are NIL.

    T do_pop(unsigned tail) noexcept {
        std::atomic<T>& q_element = details::map<SHUFFLE_BITS>(elements_, tail % size_);
        for(;;) {
            T element = q_element.exchange(NIL, R); // (2) The store to wait for.
            if(element != NIL)
                return element;
            spin_loop_pause();
        }
    }

    void do_push(T element, unsigned head) noexcept {
        assert(element != NIL);
        std::atomic<T>& q_element = details::map<SHUFFLE_BITS>(elements_, head % size_);
        for(T expected = NIL; !q_element.compare_exchange_strong(expected, element, R, X); expected = NIL)
            spin_loop_pause(); // (1) Wait for store (2) to complete.
    }

public:
    using value_type = T;

    AtomicQueue() noexcept {
        assert(std::atomic<T>{NIL}.is_lock_free());
        if(T{} != NIL)
            for(auto& element : elements_)
                element.store(NIL, X);
    }

    AtomicQueue(AtomicQueue const&) = delete;
    AtomicQueue& operator=(AtomicQueue const&) = delete;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE, bool MINIMIZE_CONTENTION = true, bool TOTAL_ORDER = false>
class AtomicQueue2 : public AtomicQueueCommon<AtomicQueue2<T, SIZE, MINIMIZE_CONTENTION, TOTAL_ORDER>> {
    using Base = AtomicQueueCommon<AtomicQueue2<T, SIZE, MINIMIZE_CONTENTION, TOTAL_ORDER>>;
    friend Base;

    enum State : unsigned char { EMPTY, STORING, STORED, LOADING };

    static constexpr unsigned size_ = MINIMIZE_CONTENTION ? details::round_up_to_power_of_2(SIZE) : SIZE;
    static constexpr int SHUFFLE_BITS =
        details::GetIndexShuffleBits<MINIMIZE_CONTENTION, size_, CACHE_LINE_SIZE / sizeof(State)>::value;
    static constexpr bool total_order_ = TOTAL_ORDER;

    alignas(CACHE_LINE_SIZE) std::atomic<unsigned char> states_[size_] = {};
    alignas(CACHE_LINE_SIZE) T elements_[size_] = {};

    T do_pop(unsigned tail) noexcept {
        unsigned index = details::remap_index<SHUFFLE_BITS>(tail % size_);
        for(;;) {
            unsigned char expected = STORED;
            if(states_[index].compare_exchange_strong(expected, LOADING, X, X)) {
                T element{std::move(elements_[index])};
                states_[index].store(EMPTY, R);
                return element;
            }
            spin_loop_pause();
        }
    }

    template<class U>
    void do_push(U&& element, unsigned head) noexcept {
        unsigned index = details::remap_index<SHUFFLE_BITS>(head % size_);
        for(;;) {
            unsigned char expected = EMPTY;
            if(states_[index].compare_exchange_strong(expected, STORING, X, X)) {
                elements_[index] = std::forward<U>(element);
                states_[index].store(STORED, R);
                return;
            }
            spin_loop_pause();
        }
    }

public:
    using value_type = T;

    AtomicQueue2() noexcept = default;
    AtomicQueue2(AtomicQueue2 const&) = delete;
    AtomicQueue2& operator=(AtomicQueue2 const&) = delete;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, class A = std::allocator<T>, T NIL = T{}, bool TOTAL_ORDER = false>
class AtomicQueueB : public AtomicQueueCommon<AtomicQueueB<T, A, NIL, TOTAL_ORDER>>,
                     private std::allocator_traits<A>::template rebind_alloc<std::atomic<T>> {
    using Base = AtomicQueueCommon<AtomicQueueB<T, A, NIL, TOTAL_ORDER>>;
    friend Base;

    static constexpr bool total_order_ = TOTAL_ORDER;

    using AllocatorElements = typename std::allocator_traits<A>::template rebind_alloc<std::atomic<T>>;

    static constexpr auto ELEMENTS_PER_CACHE_LINE = CACHE_LINE_SIZE / sizeof(std::atomic<T>);
    static_assert(ELEMENTS_PER_CACHE_LINE, "Unexpected ELEMENTS_PER_CACHE_LINE.");

    static constexpr auto SHUFFLE_BITS = details::GetCacheLineIndexBits<ELEMENTS_PER_CACHE_LINE>::value;
    static_assert(SHUFFLE_BITS, "Unexpected SHUFFLE_BITS.");

    unsigned size_;
    std::atomic<T>* elements_;

    T do_pop(unsigned tail) noexcept {
        std::atomic<T>& q_element = details::map<SHUFFLE_BITS>(elements_, tail & (size_ - 1));
        for(;;) {
            T element = q_element.exchange(NIL, R); // (2) The store to wait for.
            if(element != NIL)
                return element;
            spin_loop_pause();
        }
    }

    void do_push(T element, unsigned head) noexcept {
        assert(element != NIL);
        std::atomic<T>& q_element = details::map<SHUFFLE_BITS>(elements_, head & (size_ - 1));
        for(T expected = NIL; !q_element.compare_exchange_strong(expected, element, R, X); expected = NIL)
            spin_loop_pause(); // (1) Wait for store (2) to complete.
    }

public:
    using value_type = T;

    AtomicQueueB(unsigned size)
        : size_(std::max(details::round_up_to_power_of_2(size), 1u << (SHUFFLE_BITS * 2)))
        , elements_(AllocatorElements::allocate(size_)) {
        assert(std::atomic<T>{NIL}.is_lock_free());
        for(auto p = elements_, q = elements_ + size_; p < q; ++p)
            p->store(NIL, X);
    }

    AtomicQueueB(AtomicQueueB&& b) noexcept
        : Base(static_cast<Base&&>(b))
        , AllocatorElements(static_cast<AllocatorElements&&>(b)) // TODO: This must be noexcept, static_assert that.
        , size_(b.size_)
        , elements_(b.elements_) {
        b.size_ = 0;
        b.elements_ = 0;
    }

    AtomicQueueB& operator=(AtomicQueueB&& b) noexcept {
        b.swap(*this);
        return *this;
    }

    ~AtomicQueueB() noexcept {
        if(elements_)
            AllocatorElements::deallocate(elements_, size_); // TODO: This must be noexcept, static_assert that.
    }

    void swap(AtomicQueueB& b) noexcept {
        using std::swap;
        swap(static_cast<Base&>(*this), static_cast<Base&>(b));
        swap(static_cast<AllocatorElements&>(*this), static_cast<AllocatorElements&>(b));
        swap(size_, b.size_);
        swap(elements_, b.elements_);
    }

    friend void swap(AtomicQueueB& a, AtomicQueueB& b) {
        a.swap(b);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, class A = std::allocator<T>, bool TOTAL_ORDER = false>
class AtomicQueueB2 : public AtomicQueueCommon<AtomicQueueB2<T, A, TOTAL_ORDER>>,
                      private A,
                      private std::allocator_traits<A>::template rebind_alloc<std::atomic<uint8_t>> {
    using Base = AtomicQueueCommon<AtomicQueueB2<T, A, TOTAL_ORDER>>;
    friend Base;

    static constexpr bool total_order_ = TOTAL_ORDER;

    using AllocatorElements = A;
    using AllocatorStates = typename std::allocator_traits<A>::template rebind_alloc<std::atomic<uint8_t>>;

    enum State : uint8_t { EMPTY, STORING, STORED, LOADING };

    unsigned size_;
    std::atomic<uint8_t>* states_;
    T* elements_;

    static constexpr auto STATES_PER_CACHE_LINE = CACHE_LINE_SIZE / sizeof(State);
    static_assert(STATES_PER_CACHE_LINE, "Unexpected STATES_PER_CACHE_LINE.");

    static constexpr auto SHUFFLE_BITS = details::GetCacheLineIndexBits<STATES_PER_CACHE_LINE>::value;
    static_assert(SHUFFLE_BITS, "Unexpected SHUFFLE_BITS.");

    T do_pop(unsigned tail) noexcept {
        unsigned index = details::remap_index<SHUFFLE_BITS>(tail % (size_ - 1));
        for(;;) {
            uint8_t expected = STORED;
            if(states_[index].compare_exchange_strong(expected, LOADING, X, X)) {
                T element{std::move(elements_[index])};
                states_[index].store(EMPTY, R);
                return element;
            }
            spin_loop_pause();
        }
    }

    template<class U>
    void do_push(U&& element, unsigned head) noexcept {
        unsigned index = details::remap_index<SHUFFLE_BITS>(head % (size_ - 1));
        for(;;) {
            uint8_t expected = EMPTY;
            if(states_[index].compare_exchange_strong(expected, STORING, X, X)) {
                elements_[index] = std::forward<U>(element);
                states_[index].store(STORED, R);
                return;
            }
            spin_loop_pause();
        }
    }

public:
    using value_type = T;

    AtomicQueueB2(unsigned size)
        : size_(std::max(details::round_up_to_power_of_2(size), 1u << (SHUFFLE_BITS * 2)))
        , states_(AllocatorStates::allocate(size_))
        , elements_(AllocatorElements::allocate(size_)) {
        for(auto p = states_, q = states_ + size_; p < q; ++p)
            p->store(EMPTY, X);

        AllocatorElements& ae = *this;
        for(auto p = elements_, q = elements_ + size_; p < q; ++p)
            ae.construct(p);
    }

    AtomicQueueB2(AtomicQueueB2&& b) noexcept
        : Base(static_cast<Base&&>(b))
        , AllocatorElements(static_cast<AllocatorElements&&>(b)) // TODO: This must be noexcept, static_assert that.
        , AllocatorStates(static_cast<AllocatorStates&&>(b))     // TODO: This must be noexcept, static_assert that.
        , size_(b.size_)
        , states_(b.states_)
        , elements_(b.elements_) {
        b.size_ = 0;
        b.states_ = 0;
        b.elements_ = 0;
    }

    AtomicQueueB2& operator=(AtomicQueueB2&& b) noexcept {
        b.swap(*this);
        return *this;
    }

    ~AtomicQueueB2() noexcept {
        if(elements_) {
            AllocatorElements& ae = *this;
            for(auto p = elements_, q = elements_ + size_; p < q; ++p)
                ae.destroy(p);
            AllocatorElements::deallocate(elements_, size_); // TODO: This must be noexcept, static_assert that.

            AllocatorStates::deallocate(states_, size_); // TODO: This must be noexcept, static_assert that.
        }
    }

    void swap(AtomicQueueB2& b) noexcept {
        using std::swap;
        swap(static_cast<Base&>(*this), static_cast<Base&>(b));
        swap(static_cast<AllocatorElements&>(*this), static_cast<AllocatorElements&>(b));
        swap(static_cast<AllocatorStates&>(*this), static_cast<AllocatorStates&>(b));
        swap(size_, b.size_);
        swap(states_, b.states_);
        swap(elements_, b.elements_);
    }

    friend void swap(AtomicQueueB2& a, AtomicQueueB2& b) noexcept {
        a.swap(b);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
struct RetryDecorator : Queue {
    using T = typename Queue::value_type;

    using Queue::Queue;

    void push(T element) noexcept {
        while(!this->try_push(element))
            spin_loop_pause();
    }

    T pop() noexcept {
        T element;
        while(!this->try_pop(element))
            spin_loop_pause();
        return element;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED
