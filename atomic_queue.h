/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED
#define ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED

#include <type_traits>
#include <cassert>
#include <cstddef>
#include <utility>
#include <atomic>

// #include <emmintrin.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

int constexpr CACHE_LINE_SIZE = 64;

auto constexpr A = std::memory_order_acquire;
auto constexpr R = std::memory_order_release;
auto constexpr X = std::memory_order_relaxed;
auto constexpr C = std::memory_order_seq_cst;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace details {

template<unsigned N>
struct IsPowerOf2 {
    static bool constexpr value = !(N & (N - 1));
};

template<size_t elements_per_cache_line> struct GetCacheLineIndexBits { static int constexpr value = 0; };
template<> struct GetCacheLineIndexBits<64> { static int constexpr value = 6; };
template<> struct GetCacheLineIndexBits<32> { static int constexpr value = 5; };
template<> struct GetCacheLineIndexBits<16> { static int constexpr value = 4; };
template<> struct GetCacheLineIndexBits< 8> { static int constexpr value = 3; };
template<> struct GetCacheLineIndexBits< 4> { static int constexpr value = 2; };
template<> struct GetCacheLineIndexBits< 2> { static int constexpr value = 1; };

template<bool minimize_contention, unsigned array_size, size_t elements_per_cache_line>
struct GetIndexShuffleBits {
    static int constexpr bits = GetCacheLineIndexBits<elements_per_cache_line>::value;
    static unsigned constexpr min_size = 1u << (bits * 2);
    static int constexpr value = array_size < min_size ? 0 : bits;
    using type = std::integral_constant<int, value>;
};

template<unsigned array_size, size_t elements_per_cache_line>
struct GetIndexShuffleBits<false, array_size, elements_per_cache_line> {
    static int constexpr value = 0;
    using type = std::integral_constant<int, value>;
};

constexpr unsigned remap_index(unsigned index, std::integral_constant<int, 0>) noexcept {
    return index;
}

// Multiple writers/readers contend on the same cache line when storing/loading elements at
// subsequent indexes, aka false sharing. For power of 2 ring buffer size it is possible to re-map
// the index in such a way that each subsequent element resides on another cache line, which
// minimizes contention. This is done by swapping the lowest order N bits (which are the index of
// the element within the cache line) with the next N bits (which are the index of the cache line)
// of the element index.
template<int BITS>
constexpr unsigned remap_index(unsigned index, std::integral_constant<int, BITS>) noexcept {
    constexpr unsigned MASK = (1u << BITS) - 1;
    unsigned part0 = (index >> BITS) & MASK;
    unsigned part1 = (index & MASK) << BITS;
    unsigned part2 = index & ~(MASK | MASK << BITS);
    return part0 | part1 | part2;
}

} // details

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Derived>
class AtomicQueueCommon {
protected:
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> head_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> tail_ = {};

public:
    template<class T>
    bool try_push(T&& element) noexcept {
        auto head = head_.load(X);
        do {
            if(static_cast<int>(head - tail_.load(X)) >= static_cast<int>(Derived::size))
                return false;
        } while(!head_.compare_exchange_strong(head, head + 1, A, X));

        static_cast<Derived&>(*this).do_push(std::forward<T>(element), head);
        return true;
    }

    template<class T>
    bool try_pop(T& element) noexcept {
        auto tail = tail_.load(X);
        do {
            if(static_cast<int>(head_.load(X) - tail) <= 0)
                return false;
        } while(!tail_.compare_exchange_strong(tail, tail + 1, A, X));

        element = static_cast<Derived&>(*this).do_pop(tail);
        return true;
    }

    template<class T>
    void push(T&& element) noexcept {
        auto head = head_.fetch_add(1, A);
        static_cast<Derived&>(*this).do_push(std::forward<T>(element), head);
    }

    auto pop() noexcept {
        auto tail = tail_.fetch_add(1, A);
        return static_cast<Derived&>(*this).do_pop(tail);
    }

    bool was_empty() const noexcept {
        return static_cast<int>(head_.load(X) - tail_.load(X)) <= 0;
    }

    bool was_full() const noexcept {
        return static_cast<int>(head_.load(X) - tail_.load(X)) >= static_cast<int>(Derived::size);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<
    class T,
    unsigned SIZE,
    T NIL = T{},
    bool MinimizeContention = details::IsPowerOf2<SIZE>::value
>
class AtomicQueue : public AtomicQueueCommon<AtomicQueue<T, SIZE, NIL>> {
    alignas(CACHE_LINE_SIZE) std::atomic<T> elements_[SIZE] = {}; // Empty elements are NIL.

    friend class AtomicQueueCommon<AtomicQueue<T, SIZE, NIL>>;

    static constexpr auto size = SIZE;
    using Remap = typename details::GetIndexShuffleBits<MinimizeContention, SIZE, CACHE_LINE_SIZE / sizeof(T)>::type;

    T do_pop(unsigned tail) noexcept {
        unsigned index = details::remap_index(tail % SIZE, Remap{});
        for(;;) {
            T element = elements_[index].exchange(NIL, R);
            if(element != NIL)
                return element;
            /*_mm_pause()*/;
        }
    }

    void do_push(T element, unsigned head) noexcept {
        assert(element != NIL);
        unsigned index = details::remap_index(head % SIZE, Remap{});
        for(T expected = NIL; !elements_[index].compare_exchange_strong(expected, element, R, X); expected = NIL) // (1) Wait for store (2) to complete.
            /*_mm_pause()*/;
    }

public:
    using value_type = T;

    AtomicQueue() noexcept {
        assert(std::atomic<T>{NIL}.is_lock_free());
        if(T{} != NIL)
            for(auto& element : elements_)
                element.store(NIL, X);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<
    class T,
    unsigned SIZE,
    bool MinimizeContention = details::IsPowerOf2<SIZE>::value
    >
class AtomicQueue2 : public AtomicQueueCommon<AtomicQueue2<T, SIZE>> {
    enum State : unsigned char {
        EMPTY,
        STORING,
        STORED,
        LOADING
    };
    std::atomic<unsigned char> states_[SIZE] = {};
    alignas(CACHE_LINE_SIZE) T elements_[SIZE] = {};

    friend class AtomicQueueCommon<AtomicQueue2<T, SIZE>>;

    static constexpr auto size = SIZE;
    using Remap = typename details::GetIndexShuffleBits<MinimizeContention, SIZE, CACHE_LINE_SIZE / sizeof(unsigned char)>::type;

    T do_pop(unsigned tail) noexcept {
        unsigned index = details::remap_index(tail % SIZE, Remap{});
        for(;;) {
            unsigned char expected = STORED;
            if(states_[index].compare_exchange_strong(expected, LOADING, X, X)) {
                T element{std::move(elements_[index])};
                states_[index].store(EMPTY, R);
                return element;
            }
            /*_mm_pause()*/;
        }
    }

    template<class U>
    void do_push(U&& element, unsigned head) noexcept {
        unsigned index = details::remap_index(head % SIZE, Remap{});
        for(;;) {
            unsigned char expected = EMPTY;
            if(states_[index].compare_exchange_strong(expected, STORING, X, X)) {
                elements_[index] = std::forward<U>(element);
                states_[index].store(STORED, R);
                return;
            }
            /*_mm_pause()*/;
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

    void push(T element) noexcept {
        while(!this->try_push(element))
            /*_mm_pause()*/;
    }

    T pop() noexcept {
        T element;
        while(!this->try_pop(element))
            /*_mm_pause()*/;
        return element;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED
