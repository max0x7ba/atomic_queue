/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_DEFS_H_INCLUDED
#define ATOMIC_QUEUE_DEFS_H_INCLUDED

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include <atomic>

#if defined(__x86_64__) || defined(_M_X64) || \
    defined(__i386__) || defined(_M_IX86)
#include <emmintrin.h>
namespace atomic_queue {
constexpr int CACHE_LINE_SIZE = 64;
static inline void spin_loop_pause() noexcept {
    _mm_pause();
}
} // namespace atomic_queue
#elif defined(__arm__) || defined(__aarch64__)
namespace atomic_queue {
constexpr int CACHE_LINE_SIZE = 64;
static inline void spin_loop_pause() noexcept {
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
#else
    asm volatile ("nop" ::: "memory");
#endif
}
} // namespace atomic_queue
#else
#error "Unknown CPU architecture."
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

#if defined(__GNUC__) || defined(__clang__)
#define ATOMIC_QUEUE_LIKELY(expr) __builtin_expect(static_cast<bool>(expr), 1)
#define ATOMIC_QUEUE_UNLIKELY(expr) __builtin_expect(static_cast<bool>(expr), 0)
#else
#define ATOMIC_QUEUE_LIKELY(expr) (expr)
#define ATOMIC_QUEUE_UNLIKELY(expr) (expr)
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

auto constexpr A = std::memory_order_acquire;
auto constexpr R = std::memory_order_release;
auto constexpr X = std::memory_order_relaxed;
auto constexpr C = std::memory_order_seq_cst;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_DEFS_H_INCLUDED
