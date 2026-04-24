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
#include <utility>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

using std::uint32_t;
using std::uint64_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace details {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<size_t elements_per_cache_line> struct GetCacheLineIndexBits { static int constexpr value = 0; };
template<> struct GetCacheLineIndexBits<256> { static int constexpr value = 8; };
template<> struct GetCacheLineIndexBits<128> { static int constexpr value = 7; };
template<> struct GetCacheLineIndexBits< 64> { static int constexpr value = 6; };
template<> struct GetCacheLineIndexBits< 32> { static int constexpr value = 5; };
template<> struct GetCacheLineIndexBits< 16> { static int constexpr value = 4; };
template<> struct GetCacheLineIndexBits<  8> { static int constexpr value = 3; };
template<> struct GetCacheLineIndexBits<  4> { static int constexpr value = 2; };
template<> struct GetCacheLineIndexBits<  2> { static int constexpr value = 1; };

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Multiple writers/readers contend on the same cache line when storing/loading elements at
// subsequent indexes, aka false sharing. For power of 2 ring buffer size it is possible to re-map
// the index in such a way that each subsequent element resides on another cache line, which
// minimizes contention. This is done by swapping the lowest order N bits (which are the index of
// the element within the cache line) with the next N bits (which are the index of the cache line)
// of the element index.

template<int N_BITS>
struct IndexBits {
    enum : unsigned {
        mask_elem_idx = ~(~0u << N_BITS),
        mask_hi = ~0u << (2 * N_BITS),
        count = N_BITS
    };
};

struct RemapXor {
    // Each step depends on the previous, a serial chain of ~6 instructions, ~6 cycles.
    template<class Bits>
    ATOMIC_QUEUE_INLINE static constexpr unsigned remap(unsigned index, Bits) noexcept {
        unsigned const mix{(index ^ (index >> Bits::count)) & Bits::mask_elem_idx};
        return index ^ mix ^ (mix << Bits::count);
    }

    template<class Bits>
    ATOMIC_QUEUE_INLINE static constexpr unsigned remap(unsigned index, unsigned size, Bits b) noexcept {
        return remap(index & (size - 1), b);
    }
};

struct RemapAnd {
    // Faster index remapping with independent parallel computations of index components.
    // The shifts and ands dispatch in parallel, ~8 instructions, ~4 cycles.
    // At least +1% faster throughput benchmark relative to RemapXor.
    template<class Bits>
    ATOMIC_QUEUE_INLINE static constexpr unsigned remap(unsigned index, unsigned size, Bits) noexcept {
        return
            ((index >> Bits::count) & Bits::mask_elem_idx) |
            ((index & Bits::mask_elem_idx) << Bits::count) |
            (index & (Bits::mask_hi & (size - 1)));
    }

    template<class Bits>
    ATOMIC_QUEUE_INLINE static constexpr unsigned remap(unsigned index, Bits b) noexcept {
        return remap(index, 0, b);
    }
};

#ifdef __BMI__
struct RemapBmi {
    // Shorter and faster machine code for swapping bits with BMI instructions, if available.
    // bextr, shlx, and dispatch in parallel, ~7 instructions, ~3 cycles.
    // At least +1.5% faster throughput benchmark relative to RemapXor.
    template<class Bits>
    ATOMIC_QUEUE_INLINE static unsigned remap(unsigned index, unsigned size, Bits) noexcept {
        unsigned nn{Bits::count << 8 | Bits::count};
        // Disable constant propagation for nn to prevent the compiler from transforming the following code into more expensive instructions.
        // This one register is used by both bextr and shlx/shl instructions (otherwise transformed into different code).
        // Disable constant propagation for index too to always generate the same machine code.
        static_assert(ATOMIC_QUEUE_FULL_THROTTLE == 1, "Unexpected ATOMIC_QUEUE_FULL_THROTTLE value.");
#ifdef __BMI2__
        asm("":"+r"(nn): "r"(index)); // Any register for BMI2 shlx shift count.
#else
        asm("":"+c"(nn): "r"(index)); // Without BMI2, shl shift count must be in ecx register.
#endif

        // These 2 statements generate 2 instructions which require nn in the register.
        unsigned new_elem_idx = __builtin_ia32_bextr_u32(index, nn); // BMI1 instruction.
        // C++ standard does not define behaviour of shifts not less than the number of bits.
        // Address sanitizer reports "shift exponent X is too large for 32-bit type 'unsigned int'".
        // On x86, all dynamic shift instructions (count in a register) mask the count to 5/6 bits (32/64-bit registers).
        // (index << (nn & 31)) should compile into the same code as (index << nn), because (nn & 31) is done by dynamic shift instructions.
        // But compilers emit unnecessary (nn & 31) instructions for (index << (nn & 31)) and that's a recurring code-generation bug.
        // (index << (nn & 31)) would make Address sanitizer happy.
        unsigned new_line_idx = index << nn; // This statement generates shlx r32,r32,r32 with BMI2, otherwise shl r32,cl.

        return new_elem_idx | (new_line_idx & ~Bits::mask_hi) | (index & (Bits::mask_hi & (size - 1)));
    }

    template<class Bits>
    ATOMIC_QUEUE_INLINE static unsigned remap(unsigned index, Bits b) noexcept {
        return remap(index, 0, b);
    }
};
#endif

template<class Remap>
struct Remap0 : Remap {
    using Remap::remap;

    ATOMIC_QUEUE_INLINE static constexpr unsigned remap(unsigned index, unsigned size, IndexBits<0>) noexcept {
        return index % size;
    }

    ATOMIC_QUEUE_INLINE static constexpr unsigned remap(unsigned index, IndexBits<0>) noexcept {
        return index;
    }

    template<class Bits, class... A>
    ATOMIC_QUEUE_INLINE auto operator()(Bits bits, A... a) const noexcept {
        return this->remap(a..., bits);
    }
};

#ifdef ATOMIC_QUEUE_REMAP
// Defining ATOMIC_QUEUE_REMAP overrides the default remapper.
using Remap = Remap0<ATOMIC_QUEUE_REMAP>;
#elif defined(__BMI__)
using Remap = Remap0<RemapBmi>;
#else
using Remap = Remap0<RemapAnd>;
#endif

template<int N_BITS, class T>
ATOMIC_QUEUE_INLINE static constexpr T& remap(T* ATOMIC_QUEUE_RESTRICT elements, unsigned index, unsigned size) noexcept {
    return elements[Remap::remap(index, size, IndexBits<N_BITS>{})];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Implement a "bit-twiddling hack" for finding the next power of 2 in either 32 bits or 64 bits
// in C++11 compatible constexpr functions. The library no longer maintains C++11 compatibility.

// "Runtime" version for 32 bits
// --a;
// a |= a >> 1;
// a |= a >> 2;
// a |= a >> 4;
// a |= a >> 8;
// a |= a >> 16;
// ++a;

template<class T>
ATOMIC_QUEUE_INLINE static constexpr T decrement(T x) noexcept {
    return x - 1;
}

template<class T>
ATOMIC_QUEUE_INLINE static constexpr T increment(T x) noexcept {
    return x + 1;
}

template<class T>
ATOMIC_QUEUE_INLINE static constexpr T or_equal(T x, unsigned u) noexcept {
    return x | x >> u;
}

template<class T, class... Args>
ATOMIC_QUEUE_INLINE static constexpr T or_equal(T x, unsigned u, Args... rest) noexcept {
    return or_equal(or_equal(x, u), rest...);
}

ATOMIC_QUEUE_INLINE static constexpr uint32_t round_up_to_power_of_2(uint32_t a) noexcept {
    return increment(or_equal(decrement(a), 1, 2, 4, 8, 16));
}

ATOMIC_QUEUE_INLINE static constexpr uint64_t round_up_to_power_of_2(uint64_t a) noexcept {
    return increment(or_equal(decrement(a), 1, 2, 4, 8, 16, 32));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T>
constexpr T nil() noexcept {
#if __cpp_lib_atomic_is_always_lock_free // Better compile-time error message requires C++17.
    static_assert(std::atomic<T>::is_always_lock_free, "Queue element type T is not atomic. Use AtomicQueue2/AtomicQueueB2 for such element types.");
#endif
    return {};
}

template<class T>
ATOMIC_QUEUE_INLINE static void destroy_n(T* ATOMIC_QUEUE_RESTRICT p, unsigned n) noexcept {
    for(auto q = p + n; p != q;)
        (p++)->~T();
}

template<class T>
ATOMIC_QUEUE_INLINE static void swap_relaxed(std::atomic<T>& a, std::atomic<T>& b) noexcept {
    auto a2 = a.load(X);
    a.store(b.load(X), X);
    b.store(a2, X);
}

template<class T>
ATOMIC_QUEUE_INLINE static void copy_relaxed(std::atomic<T>& a, std::atomic<T> const& b) noexcept {
    a.store(b.load(X), X);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace details

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using State = unsigned char;
using AtomicState = std::atomic<State>;
enum StateE : State { EMPTY, STORING = 1, LOADING = 4, STORED = 128 };

template<class Derived>
class AtomicQueueCommon {
    ATOMIC_QUEUE_INLINE constexpr Derived& downcast() noexcept {
        return static_cast<Derived&>(*this);
    }

protected:
    // Put these on different cache lines to avoid false sharing between readers and writers.
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> head_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> tail_ = {};

    // The special member functions are not thread-safe.

    AtomicQueueCommon() noexcept = default;

    AtomicQueueCommon(AtomicQueueCommon const& b) noexcept
        : head_(b.head_.load(X))
        , tail_(b.tail_.load(X)) {}

    AtomicQueueCommon& operator=(AtomicQueueCommon const& b) noexcept {
        details::copy_relaxed(head_, b.head_);
        details::copy_relaxed(tail_, b.tail_);
        return *this;
    }

    // Relatively semi-special swap is not thread-safe either.
    void swap(AtomicQueueCommon& b) noexcept {
        details::swap_relaxed(head_, b.head_);
        details::swap_relaxed(tail_, b.tail_);
    }

    template<class T, T NIL>
    ATOMIC_QUEUE_INLINE static T do_pop(std::atomic<T>& q_element) noexcept {
        if(Derived::spsc_) {
            for(;;) {
                T element = q_element.load(A);
                if(ATOMIC_QUEUE_LIKELY(element != NIL)) {
                    q_element.store(NIL, X);
                    return element;
                }
                if(Derived::maximize_throughput_)
                    spin_loop_pause();
            }
        }
        else {
            for(;;) {
                T element = q_element.exchange(NIL, A); // (2) The store to wait for.
                if(ATOMIC_QUEUE_LIKELY(element != NIL))
                    return element;
                // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
                do
                    spin_loop_pause();
                while(Derived::maximize_throughput_ && q_element.load(X) == NIL);
            }
        }
    }

    template<class T, T NIL>
    ATOMIC_QUEUE_INLINE static void do_push(T element, std::atomic<T>& q_element) noexcept {
        assert(element != NIL);
        if(Derived::spsc_) {
            while(ATOMIC_QUEUE_UNLIKELY(q_element.load(X) != NIL))
                if(Derived::maximize_throughput_)
                    spin_loop_pause();
            q_element.store(element, R);
        }
        else {
            for(T expected = NIL; ATOMIC_QUEUE_UNLIKELY(!q_element.compare_exchange_weak(expected, element, R, X)); expected = NIL) {
                do
                    spin_loop_pause(); // (1) Wait for store (2) to complete.
                while(Derived::maximize_throughput_ && q_element.load(X) != NIL);
            }
        }
    }

    template<class T>
    ATOMIC_QUEUE_INLINE static T do_pop(std::atomic<State>& state, T& q_element) noexcept {
        if(Derived::spsc_) {
            while(ATOMIC_QUEUE_UNLIKELY(state.load(A) != STORED))
                if(Derived::maximize_throughput_)
                    spin_loop_pause();
            T element{std::move(q_element)};
            state.store(EMPTY, R);
            return element;
        }
        else {
            // for(;;) {
            //     unsigned char expected = STORED;
            //     if(ATOMIC_QUEUE_LIKELY(state.compare_exchange_weak(expected, LOADING, A, X))) {
            //         T element{std::move(q_element)};
            //         state.store(EMPTY, R);
            //         return element;
            //     }
            //     // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
            //     do
            //         spin_loop_pause();
            //     while(Derived::maximize_throughput_ && state.load(X) != STORED);
            // }

            // Faster code-path without compare_exchange_weak. Not ideal yet.
            // do_pop#1 may wait on do_push#1, while do_push#1 might wait on do_pop#0.
            State constexpr M = LOADING * 3 | STORED;
            while(ATOMIC_QUEUE_UNLIKELY(State(state.fetch_add(LOADING, A) & M) != STORED))
                // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
                do
                    spin_loop_pause();
                while(State(state.load(X) & M) != STORED);

            T element{std::move(q_element)};

            State e = EMPTY;
#if ATOMIC_QUEUE_FULL_THROTTLE
            asm("":"+r"(e));
#endif
            state.store(e, R);

            return element;
        }
    }

    template<class U, class T>
    ATOMIC_QUEUE_INLINE static void do_push(U&& element, std::atomic<State>& state, T& q_element) noexcept {
        if(Derived::spsc_) {
            while(ATOMIC_QUEUE_UNLIKELY(state.load(A) != EMPTY))
                if(Derived::maximize_throughput_)
                    spin_loop_pause();
            q_element = std::forward<U>(element);
            state.store(STORED, R);
        }
        else {
            // for(;;) {
            //     unsigned char expected = EMPTY;
            //     if(ATOMIC_QUEUE_LIKELY(state.compare_exchange_weak(expected, STORING, A, X))) {
            //         q_element = std::forward<U>(element);
            //         state.store(STORED, R);
            //         return;
            //     }
            //     // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
            //     do
            //         spin_loop_pause();
            //     while(Derived::maximize_throughput_ && state.load(X) != EMPTY);
            // }

            // Faster code-path without compare_exchange_weak. Not ideal yet.
            // do_push#1 might wait on do_pop#0.
            State constexpr M = STORING * 3 | STORED;
            State s;
            while(ATOMIC_QUEUE_UNLIKELY((s = State(state.fetch_add(STORING, A) & M))))
                // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
                do
                    spin_loop_pause();
                while(State(state.load(X) & M));

            q_element = std::forward<U>(element);

            // s is 0 here; (or s, STORED) is cheaper than (mov s, STORED).
#if ATOMIC_QUEUE_FULL_THROTTLE
            asm("":"+r"(s));
#endif
            s |= STORED;
            state.store(s, R);
        }
    }

public:
    template<class T>
    ATOMIC_QUEUE_INLINE bool try_push(T&& element) noexcept {
        auto head = head_.load(X);
        if(Derived::spsc_) {
            if(static_cast<int>(head - tail_.load(X)) >= static_cast<int>(downcast().size_))
                return false;
            head_.store(head + 1, X);
        }
        else {
            do {
                if(static_cast<int>(head - tail_.load(X)) >= static_cast<int>(downcast().size_))
                    return false;
            } while(ATOMIC_QUEUE_UNLIKELY(!head_.compare_exchange_weak(head, head + 1, X, X))); // This loop is not FIFO.
        }

        downcast().do_push(std::forward<T>(element), head);
        return true;
    }

    template<class T>
    ATOMIC_QUEUE_INLINE bool try_pop(T& element) noexcept {
        auto tail = tail_.load(X);
        if(Derived::spsc_) {
            if(static_cast<int>(head_.load(X) - tail) <= 0)
                return false;
            tail_.store(tail + 1, X);
        }
        else {
            do {
                if(static_cast<int>(head_.load(X) - tail) <= 0)
                    return false;
            } while(ATOMIC_QUEUE_UNLIKELY(!tail_.compare_exchange_weak(tail, tail + 1, X, X))); // This loop is not FIFO.
        }

        element = downcast().do_pop(tail);
        return true;
    }

    template<class T>
    ATOMIC_QUEUE_INLINE void push(T&& element) noexcept {
        unsigned head;
        if(Derived::spsc_) {
            head = head_.load(X);
            head_.store(head + 1, X);
        }
        else {
            constexpr auto memory_order = Derived::total_order_ ? std::memory_order_seq_cst : std::memory_order_relaxed;
            head = head_.fetch_add(1, memory_order); // FIFO and total order on Intel regardless, as of 2019.
        }
        downcast().do_push(std::forward<T>(element), head);
    }

    ATOMIC_QUEUE_INLINE auto pop() noexcept {
        unsigned tail;
        if(Derived::spsc_) {
            tail = tail_.load(X);
            tail_.store(tail + 1, X);
        }
        else {
            constexpr auto memory_order = Derived::total_order_ ? std::memory_order_seq_cst : std::memory_order_relaxed;
            tail = tail_.fetch_add(1, memory_order); // FIFO and total order on Intel regardless, as of 2019.
        }
        return downcast().do_pop(tail);
    }

    ATOMIC_QUEUE_INLINE bool was_empty() const noexcept {
        return !was_size();
    }

    ATOMIC_QUEUE_INLINE bool was_full() const noexcept {
        return was_size() >= capacity();
    }

    ATOMIC_QUEUE_INLINE unsigned was_size() const noexcept {
        // tail_ can be greater than head_ because of consumers doing pop, rather that try_pop, when the queue is empty.
        unsigned n{head_.load(X) - tail_.load(X)};
        return static_cast<int>(n) < 0 ? 0 : n; // Windows headers break std::min/max by default. Do std::max<int>(n, 0) the hard way here.
    }

    ATOMIC_QUEUE_INLINE unsigned capacity() const noexcept {
        return static_cast<Derived const&>(*this).size_;
    }

    ATOMIC_QUEUE_INLINE static constexpr bool is_spsc() noexcept {
        return Derived::spsc_;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE, T NIL = details::nil<T>(), bool MINIMIZE_CONTENTION = true, bool MAXIMIZE_THROUGHPUT = true, bool TOTAL_ORDER = false, bool SPSC = false>
class AtomicQueue : public AtomicQueueCommon<AtomicQueue<T, SIZE, NIL, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>> {
    using Base = AtomicQueueCommon<AtomicQueue<T, SIZE, NIL, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>>;
    friend Base;

    static constexpr unsigned size_ = MINIMIZE_CONTENTION ? details::round_up_to_power_of_2(SIZE) : SIZE;
    static constexpr int SHUFFLE_BITS = details::GetIndexShuffleBits<MINIMIZE_CONTENTION, size_, CACHE_LINE_SIZE / sizeof(std::atomic<T>)>::value;
    static constexpr bool total_order_ = TOTAL_ORDER;
    static constexpr bool spsc_ = SPSC;
    static constexpr bool maximize_throughput_ = MAXIMIZE_THROUGHPUT;

    alignas(CACHE_LINE_SIZE) std::atomic<T> elements_[size_];

    ATOMIC_QUEUE_INLINE T do_pop(unsigned tail) noexcept {
        std::atomic<T>& q_element = details::remap<SHUFFLE_BITS>(elements_, tail, size_);
        return Base::template do_pop<T, NIL>(q_element);
    }

    ATOMIC_QUEUE_INLINE void do_push(T element, unsigned head) noexcept {
        std::atomic<T>& q_element = details::remap<SHUFFLE_BITS>(elements_, head, size_);
        Base::template do_push<T, NIL>(element, q_element);
    }

public:
    using value_type = T;

    AtomicQueue() noexcept {
        assert(std::atomic<T>{NIL}.is_lock_free()); // Queue element type T is not atomic. Use AtomicQueue2/AtomicQueueB2 for such element types.
        for(auto p = elements_, q = elements_ + size_; p != q; ++p)
            p->store(NIL, X);
    }

    AtomicQueue(AtomicQueue const&) = delete;
    AtomicQueue& operator=(AtomicQueue const&) = delete;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE, bool MINIMIZE_CONTENTION = true, bool MAXIMIZE_THROUGHPUT = true, bool TOTAL_ORDER = false, bool SPSC = false>
class AtomicQueue2 : public AtomicQueueCommon<AtomicQueue2<T, SIZE, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>> {
    using Base = AtomicQueueCommon<AtomicQueue2<T, SIZE, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>>;
    friend Base;

    static constexpr unsigned size_ = MINIMIZE_CONTENTION ? details::round_up_to_power_of_2(SIZE) : SIZE;
    static constexpr int SHUFFLE_BITS = details::GetIndexShuffleBits<MINIMIZE_CONTENTION, size_, CACHE_LINE_SIZE / sizeof(AtomicState)>::value;
    using Bits = details::IndexBits<SHUFFLE_BITS>;
    static constexpr bool total_order_ = TOTAL_ORDER;
    static constexpr bool spsc_ = SPSC;
    static constexpr bool maximize_throughput_ = MAXIMIZE_THROUGHPUT;

    alignas(CACHE_LINE_SIZE) AtomicState states_[size_] = {};
    alignas(CACHE_LINE_SIZE) T elements_[size_] = {};

    ATOMIC_QUEUE_INLINE T do_pop(unsigned tail) noexcept {
        unsigned index = details::Remap::remap(tail, size_, Bits{});
        return Base::do_pop(states_[index], elements_[index]);
    }

    template<class U>
    ATOMIC_QUEUE_INLINE void do_push(U&& element, unsigned head) noexcept {
        unsigned index = details::Remap::remap(head, size_, Bits{});
        Base::do_push(std::forward<U>(element), states_[index], elements_[index]);
    }

public:
    using value_type = T;

    AtomicQueue2() noexcept = default;
    AtomicQueue2(AtomicQueue2 const&) = delete;
    AtomicQueue2& operator=(AtomicQueue2 const&) = delete;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, class A = std::allocator<T>, T NIL = details::nil<T>(), bool MAXIMIZE_THROUGHPUT = true, bool TOTAL_ORDER = false, bool SPSC = false>
class AtomicQueueB : private std::allocator_traits<A>::template rebind_alloc<std::atomic<T>>,
                     public AtomicQueueCommon<AtomicQueueB<T, A, NIL, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>> {
    using AllocatorElements = typename std::allocator_traits<A>::template rebind_alloc<std::atomic<T>>;
    using Base = AtomicQueueCommon<AtomicQueueB<T, A, NIL, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>>;
    friend Base;

    static constexpr bool total_order_ = TOTAL_ORDER;
    static constexpr bool spsc_ = SPSC;
    static constexpr bool maximize_throughput_ = MAXIMIZE_THROUGHPUT;

    static constexpr auto ELEMENTS_PER_CACHE_LINE = CACHE_LINE_SIZE / sizeof(std::atomic<T>);
    static_assert(ELEMENTS_PER_CACHE_LINE, "Unexpected ELEMENTS_PER_CACHE_LINE.");

    static constexpr auto SHUFFLE_BITS = details::GetCacheLineIndexBits<ELEMENTS_PER_CACHE_LINE>::value;
    static_assert(SHUFFLE_BITS, "Unexpected SHUFFLE_BITS.");
    using Bits = details::IndexBits<SHUFFLE_BITS>;

    // AtomicQueueCommon members are stored into by readers and writers.
    // Allocate these immutable members on another cache line which never gets invalidated by stores.
    alignas(CACHE_LINE_SIZE)
    unsigned size_;

    // The C++ strict aliasing rules assume that pointers to the same decayed type may alias.
    // The C++ strict aliasing rules assume that pointers to any char type may alias anything and everything.
    // A dynamically allocated array may not alias anything else by construction.
    // Explicitly annotate the circular buffer array pointer as not aliasing anything else with restrict keyword.
    std::atomic<T>* ATOMIC_QUEUE_RESTRICT elements_;

    ATOMIC_QUEUE_INLINE T do_pop(unsigned tail) noexcept {
        std::atomic<T>& q_element = details::remap<SHUFFLE_BITS>(elements_, tail, size_);
        return Base::template do_pop<T, NIL>(q_element);
    }

    ATOMIC_QUEUE_INLINE void do_push(T element, unsigned head) noexcept {
        std::atomic<T>& q_element = details::remap<SHUFFLE_BITS>(elements_, head, size_);
        Base::template do_push<T, NIL>(element, q_element);
    }

public:
    using value_type = T;
    using allocator_type = A;

    // The special member functions are not thread-safe.

    AtomicQueueB(unsigned size, A const& allocator = A{})
        : AllocatorElements(allocator)
        , size_(std::max(details::round_up_to_power_of_2(size), 1u << (SHUFFLE_BITS * 2)))
        , elements_(AllocatorElements::allocate(size_)) {
        assert(std::atomic<T>{NIL}.is_lock_free()); // Queue element type T is not atomic. Use AtomicQueue2/AtomicQueueB2 for such element types.
        std::uninitialized_fill_n(elements_, size_, NIL);
        assert(get_allocator() == allocator); // The standard requires the original and rebound allocators to manage the same state.
    }

    AtomicQueueB(AtomicQueueB&& b) noexcept
        : AllocatorElements(static_cast<AllocatorElements&&>(b)) // TODO: This must be noexcept, static_assert that.
        , Base(static_cast<Base&&>(b))
        , size_(std::exchange(b.size_, 0))
        , elements_(std::exchange(b.elements_, nullptr))
    {}

    AtomicQueueB& operator=(AtomicQueueB&& b) noexcept {
        b.swap(*this);
        return *this;
    }

    ~AtomicQueueB() noexcept {
        if(elements_) {
            details::destroy_n(elements_, size_);
            AllocatorElements::deallocate(elements_, size_); // TODO: This must be noexcept, static_assert that.
        }
    }

    A get_allocator() const noexcept {
        return *this; // The standard requires implicit conversion between rebound allocators.
    }

    void swap(AtomicQueueB& b) noexcept {
        using std::swap;
        swap(static_cast<AllocatorElements&>(*this), static_cast<AllocatorElements&>(b));
        Base::swap(b);
        swap(size_, b.size_);
        swap(elements_, b.elements_);
    }

    ATOMIC_QUEUE_INLINE friend void swap(AtomicQueueB& a, AtomicQueueB& b) noexcept {
        a.swap(b);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, class A = std::allocator<T>, bool MAXIMIZE_THROUGHPUT = true, bool TOTAL_ORDER = false, bool SPSC = false>
class AtomicQueueB2 : private std::allocator_traits<A>::template rebind_alloc<unsigned char>,
                      public AtomicQueueCommon<AtomicQueueB2<T, A, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>> {
    using StorageAllocator = typename std::allocator_traits<A>::template rebind_alloc<unsigned char>;
    using Base = AtomicQueueCommon<AtomicQueueB2<T, A, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>>;
    friend Base;

    static constexpr bool total_order_ = TOTAL_ORDER;
    static constexpr bool spsc_ = SPSC;
    static constexpr bool maximize_throughput_ = MAXIMIZE_THROUGHPUT;

    // AtomicQueueCommon members are stored into by readers and writers.
    // Allocate these immutable members on another cache line which never gets invalidated by stores.
    alignas(CACHE_LINE_SIZE)
    unsigned size_;

    // The C++ strict aliasing rules assume that pointers to the same decayed type may alias.
    // The C++ strict aliasing rules assume that pointers to any char type may alias anything and everything.
    // A dynamically allocated array may not alias anything else by construction.
    // Explicitly annotate the circular buffer array pointers as not aliasing anything else with restrict keyword.
    AtomicState* ATOMIC_QUEUE_RESTRICT states_;
    T* ATOMIC_QUEUE_RESTRICT elements_;

    static constexpr auto STATES_PER_CACHE_LINE = CACHE_LINE_SIZE / sizeof(AtomicState);
    static_assert(STATES_PER_CACHE_LINE, "Unexpected STATES_PER_CACHE_LINE.");

    static constexpr auto SHUFFLE_BITS = details::GetCacheLineIndexBits<STATES_PER_CACHE_LINE>::value;
    static_assert(SHUFFLE_BITS, "Unexpected SHUFFLE_BITS.");
    using Bits = details::IndexBits<SHUFFLE_BITS>;

    ATOMIC_QUEUE_INLINE T do_pop(unsigned tail) noexcept {
        unsigned index = details::Remap::remap(tail, size_, Bits{});
        return Base::do_pop(states_[index], elements_[index]);
    }

    template<class U>
    ATOMIC_QUEUE_INLINE void do_push(U&& element, unsigned head) noexcept {
        unsigned index = details::Remap::remap(head, size_, Bits{});
        Base::do_push(std::forward<U>(element), states_[index], elements_[index]);
    }

    template<class U>
    U* allocate_() {
        U* p = reinterpret_cast<U*>(StorageAllocator::allocate(size_ * sizeof(U)));
        assert(reinterpret_cast<uintptr_t>(p) % alignof(U) == 0); // Allocated storage must be suitably aligned for U.
        return p;
    }

    template<class U>
    void deallocate_(U* p) noexcept {
        StorageAllocator::deallocate(reinterpret_cast<unsigned char*>(p), size_ * sizeof(U)); // TODO: This must be noexcept, static_assert that.
    }

public:
    using value_type = T;
    using allocator_type = A;

    // The special member functions are not thread-safe.

    AtomicQueueB2(unsigned size, A const& allocator = A{})
        : StorageAllocator(allocator)
        , size_(std::max(details::round_up_to_power_of_2(size), 1u << (SHUFFLE_BITS * 2)))
        , states_(allocate_<AtomicState>())
        , elements_(allocate_<T>()) {
        std::uninitialized_fill_n(states_, size_, EMPTY);
        A a = get_allocator();
        assert(a == allocator); // The standard requires the original and rebound allocators to manage the same state.
        for(auto p = elements_, q = elements_ + size_; p < q; ++p)
            std::allocator_traits<A>::construct(a, p);
    }

    AtomicQueueB2(AtomicQueueB2&& b) noexcept
        : StorageAllocator(static_cast<StorageAllocator&&>(b)) // TODO: This must be noexcept, static_assert that.
        , Base(static_cast<Base&&>(b))
        , size_(std::exchange(b.size_, 0))
        , states_(std::exchange(b.states_, nullptr))
        , elements_(std::exchange(b.elements_, nullptr))
    {}

    AtomicQueueB2& operator=(AtomicQueueB2&& b) noexcept {
        b.swap(*this);
        return *this;
    }

    ~AtomicQueueB2() noexcept {
        if(elements_) {
            A a = get_allocator();
            for(auto p = elements_, q = elements_ + size_; p < q; ++p)
                std::allocator_traits<A>::destroy(a, p);
            deallocate_(elements_);
            details::destroy_n(states_, size_);
            deallocate_(states_);
        }
    }

    A get_allocator() const noexcept {
        return *this; // The standard requires implicit conversion between rebound allocators.
    }

    void swap(AtomicQueueB2& b) noexcept {
        using std::swap;
        swap(static_cast<StorageAllocator&>(*this), static_cast<StorageAllocator&>(b));
        Base::swap(b);
        swap(size_, b.size_);
        swap(states_, b.states_);
        swap(elements_, b.elements_);
    }

    ATOMIC_QUEUE_INLINE friend void swap(AtomicQueueB2& a, AtomicQueueB2& b) noexcept {
        a.swap(b);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED
