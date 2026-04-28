/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_DEFS_H_INCLUDED
#define ATOMIC_QUEUE_DEFS_H_INCLUDED

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include <atomic>
#include <cstdint>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(__GNUC__) || defined(__clang__)
#define ATOMIC_QUEUE_LIKELY(expr) __builtin_expect(static_cast<bool>(expr), 1)
#define ATOMIC_QUEUE_UNLIKELY(expr) __builtin_expect(static_cast<bool>(expr), 0)
#define ATOMIC_QUEUE_NOINLINE __attribute__((noinline))
#define ATOMIC_QUEUE_INLINE inline __attribute__((always_inline))
#define ATOMIC_QUEUE_RESTRICT __restrict__
#define ATOMIC_QUEUE_FULL_THROTTLE 1
#else
#define ATOMIC_QUEUE_FULL_THROTTLE 0
#define ATOMIC_QUEUE_LIKELY(expr) (expr)
#define ATOMIC_QUEUE_UNLIKELY(expr) (expr)
#define ATOMIC_QUEUE_NOINLINE
#define ATOMIC_QUEUE_INLINE inline
#ifdef _MSC_VER
#define ATOMIC_QUEUE_RESTRICT __restrict
#else
#define ATOMIC_QUEUE_RESTRICT
#endif
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Define a CPU-specific spin_loop_pause function.

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <emmintrin.h>
#endif

namespace atomic_queue {

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
constexpr int CACHE_LINE_SIZE = 64;
ATOMIC_QUEUE_INLINE static void spin_loop_pause() noexcept {
    _mm_pause();
}

#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM64)
/*
TODO:

"asm volatile" is adequate and sufficient to prevent the asm statements from being reordered by a compiler relative to any and all preceding and following statements.

The "memory" clobber in an asm statement invalidates all memory and all registers loaded from memory prior to the asm statement, forcing compilers to emit otherwise unnecessary machine code to reload registers from memory. "memory" clobbers is one of the worst performance killers -- a key motivation for inventing std::memory_order. std::memory_order is the extreme opposite of asm "memory" clobbers enabling fine-grained control of non-atomic instruction reordering relative to atomic instructions, obviating the need to ever use the most detrimental and undesirable asm "memory" clobbers.

The asm "memory" clobbers defeat and undo any and all positive effects of the precise weakest and cheapest possible std::memory_order arguments this library calls std::atomic member functions with. The benchmarks built for ARM are unlikely to perform anywhere near/similar to the x86_64 benchmark levels of performance with the asm "memory" clobber.

The effects of asm "memory" clobber have only recently become intuitively familiar to me and I don't have access to a multi-core ARM workstation to benchmark the performance boost of removing the asm "memory" clobber. Which I expect to be significant, based on my experience of relaxing std::memory_orders on x86_64 platform. Hence, benchmarking on multi-core ARM hardware is required to validate/justify removing the "memory" clobber.
*/

constexpr int CACHE_LINE_SIZE = 64;
ATOMIC_QUEUE_INLINE static void spin_loop_pause() noexcept {
#if (defined(__ARM_ARCH_6K__) || \
     defined(__ARM_ARCH_6Z__) || \
     defined(__ARM_ARCH_6ZK__) || \
     defined(__ARM_ARCH_6T2__) || \
     defined(__ARM_ARCH_7__) || \
     defined(__ARM_ARCH_7A__) || \
     defined(__ARM_ARCH_7R__) || \
     defined(__ARM_ARCH_7M__) || \
     defined(__ARM_ARCH_7S__) || \
     defined(__ARM_ARCH_8A__) || \
     defined(__aarch64__))
    asm volatile ("yield" ::: "memory");
#elif defined(_M_ARM64)
    __yield();
#else
    asm volatile ("nop" ::: "memory");
#endif
}

#elif defined(__ppc64__) || defined(__powerpc64__)
constexpr int CACHE_LINE_SIZE = 128;
ATOMIC_QUEUE_INLINE static void spin_loop_pause() noexcept {
    asm volatile("or 31,31,31 # very low priority");
}

#elif defined(__s390x__)
constexpr int CACHE_LINE_SIZE = 256;
ATOMIC_QUEUE_INLINE static void spin_loop_pause() noexcept {} // TODO: Find the right instruction to use here, if any.

#elif defined(__riscv)
constexpr int CACHE_LINE_SIZE = 64;
ATOMIC_QUEUE_INLINE static void spin_loop_pause() noexcept {
    asm volatile (".insn i 0x0F, 0, x0, x0, 0x010");
}

#elif defined(__loongarch__)
constexpr int CACHE_LINE_SIZE = 64;
ATOMIC_QUEUE_INLINE static void spin_loop_pause() noexcept {
    asm volatile("nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop");
}

#else
#ifdef _MSC_VER
#pragma message("Unknown CPU architecture. Using L1 cache line size of 64 bytes and no spinloop pause instruction.")
#else
#warning "Unknown CPU architecture. Using L1 cache line size of 64 bytes and no spinloop pause instruction."
#endif

constexpr int CACHE_LINE_SIZE = 64; // TODO: Review that this is the correct value.
ATOMIC_QUEUE_INLINE static void spin_loop_pause() noexcept {}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

auto constexpr A = std::memory_order_acquire;
auto constexpr R = std::memory_order_release;
auto constexpr X = std::memory_order_relaxed;
auto constexpr C = std::memory_order_seq_cst;
auto constexpr AR = std::memory_order_acq_rel;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ATOMIC_QUEUE_INLINE static constexpr int as_signed(unsigned c) noexcept { return c; }
ATOMIC_QUEUE_INLINE static constexpr int as_signed(int c) noexcept { return c; }
ATOMIC_QUEUE_INLINE static constexpr unsigned as_unsigned(unsigned c) noexcept { return c; }
ATOMIC_QUEUE_INLINE static constexpr unsigned as_unsigned(int c) noexcept { return c; }

// Do not allow integral promotion, numeric conversions or any other conversions for arguments of as_signed and as_unsigned.
template<class T> T as_signed(T) noexcept = delete;
template<class T> T as_unsigned(T) noexcept = delete;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// std::min/max reference parameters may require spilling registers to stack in order to make a value addressable/referencable.
// These take by value only, no conversions.

template<class T>
ATOMIC_QUEUE_INLINE static T min_value(T a, T b) noexcept {
    return b < a ? b : a;
}

template<class T>
ATOMIC_QUEUE_INLINE static T max_value(T a, T b) noexcept {
    return a < b ? b : a;
}

// Let the caller resolve any ambiguity.
template<class T, class U> T min_value(T a, T b) noexcept = delete;
template<class T, class U> T max_value(T a, T b) noexcept = delete;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T>
ATOMIC_QUEUE_INLINE static constexpr bool is_suitably_aligned(T* p) noexcept {
    return !(reinterpret_cast<std::uintptr_t>(p) % alignof(T));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct NoContext {
    template<class... Args>
    ATOMIC_QUEUE_INLINE constexpr NoContext(Args&&...) noexcept {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_DEFS_H_INCLUDED
