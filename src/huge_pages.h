/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef HUGE_PAGES_H_INCLUDED
#define HUGE_PAGES_H_INCLUDED

#include "atomic_queue/defs.h"

#include <new>
#include <memory>
#include <utility>
#include <cassert>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class HugePages {
    unsigned char* cur_;
    unsigned char* end_;
    unsigned char* beg_;

public:
    using WarnFn = void();
    static WarnFn* warn_no_1GB_pages;
    static WarnFn* warn_no_2MB_pages;

    static HugePages* instance;

    struct Deleter {
        template<class T>
        ATOMIC_QUEUE_INLINE void operator()(T* p) const {
            instance->destroy(p);
        }
    };

    template<class T>
    using unique_ptr = std::unique_ptr<T, Deleter>;

    enum Type { PAGE_DEFAULT = 0, PAGE_2MB = 21, PAGE_1GB = 30 };

    HugePages(Type t, size_t total_size);
    ~HugePages() noexcept;

    HugePages(HugePages const&) = delete;
    HugePages& operator=(HugePages const&) = delete;

    ATOMIC_QUEUE_INLINE HugePages(HugePages&& b) noexcept
        : cur_(b.cur_)
        , end_(b.end_)
        , beg_(b.beg_)
    {
        b.beg_ = 0;
    }

    ATOMIC_QUEUE_INLINE HugePages& operator=(HugePages&& b) noexcept {
        b.swap(*this);
        return *this;
    }

    void* allocate(size_t size, std::nothrow_t) noexcept;
    void* allocate(size_t size);
    void deallocate(void* p, size_t size) noexcept;
    void check_huge_pages_leaks(char const* name) noexcept;

    ATOMIC_QUEUE_INLINE void reset() noexcept {
        cur_ = beg_;
    }

    ATOMIC_QUEUE_INLINE bool empty() const noexcept {
        return cur_ == beg_;
    }

    ATOMIC_QUEUE_INLINE size_t available() const noexcept {
        return end_ - cur_;
    }

    ATOMIC_QUEUE_INLINE size_t capacity() const noexcept {
        return end_ - beg_;
    }

    ATOMIC_QUEUE_INLINE size_t used() const noexcept {
        return cur_ - beg_;
    }

    template<class T, class... Args>
    ATOMIC_QUEUE_INLINE T* create(Args&&... args) {
        return new(this->allocate(sizeof(T))) T{std::forward<Args>(args)...};
    }

    template<class T, class... Args>
    ATOMIC_QUEUE_INLINE unique_ptr<T> create_unique_ptr(Args&&... args) {
        return unique_ptr<T>{create<T>(std::forward<Args>(args)...)};
    }

    template<class T, class... Args>
    ATOMIC_QUEUE_INLINE unique_ptr<T> create_unique_ptr(NoContext, Args&&... args) {
        return unique_ptr<T>{create<T>(std::forward<Args>(args)...)};
    }

    template<class T>
    ATOMIC_QUEUE_INLINE void destroy(T* p) {
        void* q = p;
        p->~T();
        this->deallocate(q, sizeof(T));
    }

    ATOMIC_QUEUE_INLINE void swap(HugePages& b) noexcept {
        using std::swap;
        swap(cur_, b.cur_);
        swap(end_, b.end_);
        swap(beg_, b.beg_);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template<class T>
struct HugePageAllocator { // A stateless allocator.
    template<class U> struct rebind { using other = HugePageAllocator<U>; };

    using value_type = T;

    HugePageAllocator() noexcept = default;

    template<class U>
    ATOMIC_QUEUE_INLINE HugePageAllocator(HugePageAllocator<U>) noexcept
    {}

    ATOMIC_QUEUE_INLINE T* allocate(size_t n) const {
        T* p = static_cast<T*>(HugePages::instance->allocate(n * sizeof(T)));
        assert(is_suitably_aligned(p));
        return p;
    }

    ATOMIC_QUEUE_INLINE void deallocate(T* p, size_t n) const {
        HugePages::instance->deallocate(p, n * sizeof(T));
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // HUGE_PAGES_H_INCLUDED
