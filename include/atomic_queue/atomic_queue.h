/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED
#define ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "defs.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <utility>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace details {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using std::uint32_t;
using std::uint64_t;

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

template<unsigned N_BITS>
struct IndexBits {
    enum : unsigned {
        mask_elem_idx = ~(~0u << N_BITS),
        mask_line_idx = mask_elem_idx << N_BITS,
        mask_hi = ~0u << (2 * N_BITS),
        count = N_BITS,
        count2 = N_BITS << 8 | N_BITS,
    };
};

struct RemapXor {
    // Each step depends on the previous, a serial chain of ~6 instructions, ~6 cycles.
    template<class B>
    ATOMIC_QUEUE_SINLINE constexpr unsigned remap(unsigned index, B) noexcept {
        unsigned const mix{(index ^ (index >> B::count)) & B::mask_elem_idx};
        return index ^ mix ^ (mix << B::count);
    }

    template<class B>
    ATOMIC_QUEUE_SINLINE constexpr unsigned remap(unsigned index, unsigned size, B b) noexcept {
        return remap(index & (size - 1), b);
    }
};

struct RemapAnd {
    // Faster index remapping with independent parallel computations of index components.
    // The shifts and ands dispatch in parallel, ~8 instructions, ~4 cycles.
    // At least +1% faster throughput benchmark relative to RemapXor.
    template<class B>
    ATOMIC_QUEUE_SINLINE constexpr unsigned remap(unsigned index, unsigned size, B) noexcept {
        return
            ((index >> B::count) & B::mask_elem_idx) |
            ((index & B::mask_elem_idx) << B::count) |
            (index & (B::mask_hi & (size - 1)));
    }

    template<class B>
    ATOMIC_QUEUE_SINLINE constexpr unsigned remap(unsigned index, B b) noexcept {
        return remap(index, 0, b);
    }
};

#ifdef __BMI__
struct RemapBmi {
    // Shorter and faster machine code for swapping bits with BMI instructions, if available.
    // BMI1 (and, bextr, mov + and) dispatch in parallel, 7 instructions, ~3 cycles.
    // BMI2 (and, bextr, bzhi) dispatch in parallel, 6 instructions, ~3 cycles.
    // At least +1.5% faster throughput benchmark relative to RemapXor.
    template<class B>
    ATOMIC_QUEUE_SINLINE unsigned remap(unsigned index, unsigned size, B) noexcept {
        static_assert(ATOMIC_QUEUE_FULL_THROTTLE == 1, "Unexpected ATOMIC_QUEUE_FULL_THROTTLE value.");
        unsigned nn  = B::count2;
        ATOMIC_QUEUE_REG(nn); // Disable constant propagation for nn to prevent the compiler from transforming the following code.

#ifdef __BMI2__
        unsigned new_line_idx = _bzhi_u32(index, nn) << B::count; // BMI2 bzhi supersedes mov + and.
        // unsigned new_line_idx = (index << nn) & B::mask_line_idx; // BMI2 shlx supersedes mov + shl.
        unsigned new_elem_idx = __bextr_u32(index, nn); // BMI1 bextr supersedes mov + shr + and.
#else
        unsigned new_elem_idx = __bextr_u32(index, nn); // BMI1 bextr supersedes mov + shr + and.
        unsigned new_line_idx = (index & B::mask_elem_idx) << B::count;
#endif

        new_elem_idx |= index & (B::mask_hi & (size - 1));
        ATOMIC_QUEUE_ORDER(new_elem_idx, new_line_idx); // Do not commute the arguments of the adjacent two or instructions.
        return new_elem_idx | new_line_idx; // Or with new_line_idx last.
    }

    template<class B>
    ATOMIC_QUEUE_SINLINE unsigned remap(unsigned index, B b) noexcept {
        return remap(index, 0, b);
    }
};
#endif // __BMI__

template<class Remap>
struct Remap0 : Remap {
    using Remap::remap;

    ATOMIC_QUEUE_SINLINE constexpr unsigned remap(unsigned index, unsigned size, IndexBits<0>) noexcept {
        return index % size;
    }

    ATOMIC_QUEUE_SINLINE constexpr unsigned remap(unsigned index, IndexBits<0>) noexcept {
        return index;
    }

    template<class B, class... A>
    ATOMIC_QUEUE_INLINE auto operator()(B bits, A... a) const noexcept {
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

template<unsigned N_BITS>
ATOMIC_QUEUE_SINLINE constexpr unsigned remap(unsigned index, unsigned size, IndexBits<N_BITS> b) noexcept {
    return Remap::remap(index, size, b);
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
ATOMIC_QUEUE_SINLINE constexpr T decrement(T x) noexcept {
    return x - 1;
}

template<class T>
ATOMIC_QUEUE_SINLINE constexpr T increment(T x) noexcept {
    return x + 1;
}

template<class T>
ATOMIC_QUEUE_SINLINE constexpr T or_equal(T x, unsigned u) noexcept {
    return x | x >> u;
}

template<class T, class... Args>
ATOMIC_QUEUE_SINLINE constexpr T or_equal(T x, unsigned u, Args... rest) noexcept {
    return or_equal(or_equal(x, u), rest...);
}

ATOMIC_QUEUE_SINLINE constexpr uint32_t round_up_to_power_of_2(uint32_t a) noexcept {
    return increment(or_equal(decrement(a), 1, 2, 4, 8, 16));
}

ATOMIC_QUEUE_SINLINE constexpr uint64_t round_up_to_power_of_2(uint64_t a) noexcept {
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
ATOMIC_QUEUE_SINLINE void destroy_n(T* ATOMIC_QUEUE_RESTRICT p, unsigned n) noexcept {
    for(auto q = p + n; p != q;)
        (p++)->~T();
}

template<class T>
ATOMIC_QUEUE_SINLINE void swap_relaxed(std::atomic<T>& a, std::atomic<T>& b) noexcept {
    auto a2 = a.load(X);
    a.store(b.load(X), X);
    b.store(a2, X);
}

template<class T>
ATOMIC_QUEUE_SINLINE void copy_relaxed(std::atomic<T>& a, std::atomic<T> const& b) noexcept {
    a.store(b.load(X), X);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace details

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using State = unsigned char;
using AtomicState = std::atomic<State>;

enum StateE : State {
    EMPTY,
    STORED = 1,
    STORING = 2,
    LOADING = 4
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Derived>
class AtomicQueueCommon {
    ATOMIC_QUEUE_INLINE constexpr auto& downcast() noexcept { return static_cast<Derived&>(*this); }
    ATOMIC_QUEUE_INLINE constexpr auto& downcast() const noexcept { return static_cast<Derived const&>(*this); }

protected:
    // Put these on different cache lines to avoid false sharing between readers and writers.
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> head_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> tail_ = {};

    // The special member functions are not thread-safe.

    AtomicQueueCommon() noexcept {
        assert(is_suitably_aligned(&downcast()));
    }

    AtomicQueueCommon(AtomicQueueCommon const& b) noexcept
        : head_(b.head_.load(X))
        , tail_(b.tail_.load(X))
    {
        assert(is_suitably_aligned(&downcast()));
    }

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

    template<class T>
    ATOMIC_QUEUE_SINLINE T do_pop(std::atomic<T>* ATOMIC_QUEUE_RESTRICT elements, unsigned index) noexcept {
        constexpr T NIL = Derived::nil_;
        T element;
        auto& q_element = elements[index];

        if(Derived::spsc_) {
            for(;;) {
                element = q_element.load(A);
                if(ATOMIC_QUEUE_LIKELY(element != NIL))
                    break;
                if(Derived::maximize_throughput_)
                    spin_loop_pause();
            }
            q_element.store(NIL, R);
        }
        else {
            for(;;) {
                element = q_element.exchange(NIL, AR); // (2) The store to wait for.
                if(ATOMIC_QUEUE_LIKELY(element != NIL))
                    break;
                // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
                do
                    spin_loop_pause();
                while(ATOMIC_QUEUE_UNLIKELY(Derived::maximize_throughput_ && q_element.load(X) == NIL));
            }
        }
        return element;
    }

    template<class T>
    ATOMIC_QUEUE_SINLINE void do_push(T element, std::atomic<T>* ATOMIC_QUEUE_RESTRICT elements, unsigned index) noexcept {
        constexpr T NIL = Derived::nil_;
        assert(element != NIL);
        auto& q_element = elements[index];

        if(Derived::spsc_) {
            while(ATOMIC_QUEUE_UNLIKELY(q_element.load(A) != NIL)) // Hint the branch as not taken when the queue is not full.
                if(Derived::maximize_throughput_)
                    spin_loop_pause();
            q_element.store(element, R);
        }
        else {
            T expected;
            while(ATOMIC_QUEUE_UNLIKELY(!q_element.compare_exchange_weak((expected = NIL), element, AR, X))) // Hint the branch as not taken when the queue is not full.
                do // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
                    spin_loop_pause(); // (1) Wait for store (2) to complete.
                while(ATOMIC_QUEUE_UNLIKELY(Derived::maximize_throughput_ && q_element.load(X) != NIL));
        }
    }

    template<class T>
    ATOMIC_QUEUE_SINLINE T do_pop(std::atomic<State>* ATOMIC_QUEUE_RESTRICT states, T* ATOMIC_QUEUE_RESTRICT elements, unsigned index) noexcept {
        auto& state = states[index];

        if(Derived::spsc_) {
            while(ATOMIC_QUEUE_UNLIKELY(state.load(A) != STORED)) // Hint the branch as not taken when the queue is not empty.
                if(Derived::maximize_throughput_)
                    spin_loop_pause();
        }
        else {
            State expected, desired = LOADING;
            ATOMIC_QUEUE_LEAN_REG(desired);
            while(ATOMIC_QUEUE_UNLIKELY(!state.compare_exchange_weak((expected = STORED), desired, A, X))) { // Hint the branch as not taken when the queue is not empty.
                do // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
                    spin_loop_pause();
                while(ATOMIC_QUEUE_UNLIKELY(Derived::maximize_throughput_ && state.load(X) != STORED));
                ATOMIC_QUEUE_LEAN_REG(desired);
            }
        }

        T element{std::move(elements[index])};
        state.store(EMPTY, R);
        return element;
    }

    template<class U, class T>
    ATOMIC_QUEUE_SINLINE void do_push(U&& element, std::atomic<State>* ATOMIC_QUEUE_RESTRICT states, T* ATOMIC_QUEUE_RESTRICT elements, unsigned index) noexcept {
        auto& state = states[index];

        if(Derived::spsc_) {
            while(ATOMIC_QUEUE_UNLIKELY(state.load(A) != EMPTY)) // Hint the branch as not taken when the queue is not full.
                if(Derived::maximize_throughput_)
                    spin_loop_pause();
        }
        else {
            State expected, desired = STORING;
            ATOMIC_QUEUE_LEAN_REG(desired);
            while(ATOMIC_QUEUE_UNLIKELY(!state.compare_exchange_weak((expected = EMPTY), desired, A, X))) {// Hint the branch as not taken when the queue is not full.
                do // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
                    spin_loop_pause();
                while(ATOMIC_QUEUE_UNLIKELY(Derived::maximize_throughput_ && state.load(X) != EMPTY));
                ATOMIC_QUEUE_LEAN_REG(desired);
            }
        }

        elements[index] = std::forward<U>(element);
        state.store(STORED, R);
    }

public:
    template<class T>
    ATOMIC_QUEUE_INLINE bool try_push(T&& element) noexcept {
        auto head = head_.load(X);
        if(Derived::spsc_) {
            if(ATOMIC_QUEUE_UNLIKELY(as_signed(head - tail_.load(X)) >= as_signed(downcast().size_)))
                return false;
            head_.store(head + 1, X);
        }
        else {
            do {
                if(ATOMIC_QUEUE_UNLIKELY(as_signed(head - tail_.load(X)) >= as_signed(downcast().size_)))
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
            if(ATOMIC_QUEUE_UNLIKELY(as_signed(head_.load(X) - tail) <= 0))
                return false;
            tail_.store(tail + 1, X);
        }
        else {
            do {
                if(ATOMIC_QUEUE_UNLIKELY(as_signed(head_.load(X) - tail) <= 0))
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
        return max_value(as_signed(n), 0);
    }

    ATOMIC_QUEUE_INLINE unsigned capacity() const noexcept {
        return downcast().size_;
    }

    ATOMIC_QUEUE_SINLINE constexpr bool is_spsc() noexcept {
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
    using B = details::IndexBits<SHUFFLE_BITS>;
    static constexpr bool total_order_ = TOTAL_ORDER;
    static constexpr bool spsc_ = SPSC;
    static constexpr bool maximize_throughput_ = MAXIMIZE_THROUGHPUT;
    static constexpr T nil_ = NIL;

    alignas(CACHE_LINE_SIZE) std::atomic<T> elements_[size_];

    ATOMIC_QUEUE_INLINE T do_pop(unsigned tail) noexcept {
        auto index = remap(tail, size_, B{});
        return Base::do_pop(elements_, index);
    }

    ATOMIC_QUEUE_INLINE void do_push(T element, unsigned head) noexcept {
        auto index = remap(head, size_, B{});
        Base::do_push(element, elements_, index);
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
    using B = details::IndexBits<SHUFFLE_BITS>;
    static constexpr bool total_order_ = TOTAL_ORDER;
    static constexpr bool spsc_ = SPSC;
    static constexpr bool maximize_throughput_ = MAXIMIZE_THROUGHPUT;

    alignas(CACHE_LINE_SIZE) AtomicState states_[size_] = {};
    alignas(CACHE_LINE_SIZE) T elements_[size_] = {};

    ATOMIC_QUEUE_INLINE T do_pop(unsigned tail) noexcept {
        auto index = remap(tail, size_, B{});
        return Base::do_pop(states_, elements_, index);
    }

    template<class U>
    ATOMIC_QUEUE_INLINE void do_push(U&& element, unsigned head) noexcept {
        auto index = remap(head, size_, B{});
        Base::do_push(std::forward<U>(element), states_, elements_, index);
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
    static constexpr T nil_ = NIL;

    static constexpr auto ELEMENTS_PER_CACHE_LINE = CACHE_LINE_SIZE / sizeof(std::atomic<T>);
    static_assert(ELEMENTS_PER_CACHE_LINE, "Unexpected ELEMENTS_PER_CACHE_LINE.");

    static constexpr auto SHUFFLE_BITS = details::GetCacheLineIndexBits<ELEMENTS_PER_CACHE_LINE>::value;
    static_assert(SHUFFLE_BITS, "Unexpected SHUFFLE_BITS.");
    using B = details::IndexBits<SHUFFLE_BITS>;

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
        auto index = remap(tail, size_, B{});
        return Base::do_pop(elements_, index);
    }

    ATOMIC_QUEUE_INLINE void do_push(T element, unsigned head) noexcept {
        auto index = remap(head, size_, B{});
        Base::do_push(element, elements_, index);
    }

public:
    using value_type = T;
    using allocator_type = A;

    // The special member functions are not thread-safe.

    AtomicQueueB(unsigned size, A const& allocator = A{})
        : AllocatorElements(allocator)
        , size_(max_value(details::round_up_to_power_of_2(size), 1u << (SHUFFLE_BITS * 2)))
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
    using B = details::IndexBits<SHUFFLE_BITS>;

    ATOMIC_QUEUE_INLINE T do_pop(unsigned tail) noexcept {
        auto index = remap(tail, size_, B{});
        return Base::do_pop(states_, elements_, index);
    }

    template<class U>
    ATOMIC_QUEUE_INLINE void do_push(U&& element, unsigned head) noexcept {
        auto index = remap(head, size_, B{});
        Base::do_push(std::forward<U>(element), states_, elements_, index);
    }

    template<class U>
    U* allocate_() {
        U* p = reinterpret_cast<U*>(StorageAllocator::allocate(size_ * sizeof(U)));
        assert(is_suitably_aligned(p)); // Allocated storage must be suitably aligned for U.
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
        , size_(max_value(details::round_up_to_power_of_2(size), 1u << (SHUFFLE_BITS * 2)))
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
