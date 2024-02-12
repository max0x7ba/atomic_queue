/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef HUGE_PAGES_H_INCLUDED
#define HUGE_PAGES_H_INCLUDED

#include "benchmarks.h"

#include <new>
#include <memory>
#include <utility>

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

    struct Deleter {
        HugePages* hp_;

        template<class T>
        void operator()(T* p) const {
            hp_->destroy(p);
        }
    };

    enum Type { PAGE_DEFAULT = 0, PAGE_2MB = 21, PAGE_1GB = 30 };

    HugePages(Type t, size_t total_size);
    ~HugePages() noexcept;

    HugePages(HugePages const&) = delete;
    HugePages& operator=(HugePages const&) = delete;

    HugePages(HugePages&& b) noexcept
        : cur_(b.cur_)
        , end_(b.end_)
        , beg_(b.beg_)
    {
        b.beg_ = 0;
    }

    HugePages& operator=(HugePages&& b) noexcept {
        b.swap(*this);
        return *this;
    }

    void reset() noexcept {
        cur_ = beg_;
    }

    bool empty() const noexcept {
        return cur_ == beg_;
    }

    size_t available() const noexcept {
        return end_ - cur_;
    }

    size_t capacity() const noexcept {
        return end_ - beg_;
    }

    size_t used() const noexcept {
        return cur_ - beg_;
    }

    void* allocate(size_t size, std::nothrow_t) noexcept {
        if(static_cast<size_t>(end_ - cur_) < size)
            return 0;
        void* p = cur_;
        cur_ += size;
        return p;
    }

    void* allocate(size_t size) {
        void* p = this->allocate(size, std::nothrow_t{});
        if(!p)
            throw std::bad_alloc();
        return p;
    }

    void deallocate(void* p, size_t size) noexcept {
        auto q = cur_ - size;
        if(q == p) // Can only deallocate the last allocation at the end.
            cur_ = q;
    }

    template<class T, class... Args>
    T* create(Args&&... args) {
        return new(this->allocate(sizeof(T))) T{std::forward<Args>(args)...};
    }

    template<class T, class... Args>
    std::unique_ptr<T, Deleter> create_unique_ptr(Args&&... args) {
        return std::unique_ptr<T, Deleter>{new(this->allocate(sizeof(T))) T{std::forward<Args>(args)...}, Deleter{this}};
    }

    template<class T, class... Args>
    std::unique_ptr<T, Deleter> create_unique_ptr(NoContext, Args&&... args) {
        return std::unique_ptr<T, Deleter>{new(this->allocate(sizeof(T))) T{std::forward<Args>(args)...}, Deleter{this}};
    }

    template<class T>
    void destroy(T* p) {
        void* q = p;
        p->~T();
        this->deallocate(q, sizeof(T));
    }

    void swap(HugePages& b) noexcept {
        using std::swap;
        swap(cur_, b.cur_);
        swap(end_, b.end_);
        swap(beg_, b.beg_);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct HugePageAllocatorBase {
    static HugePages* hp;
};

template<class T>
struct HugePageAllocator : HugePageAllocatorBase
{
    // static bool constexpr is_always_equal = false;
    // static bool constexpr propagate_on_container_copy_assignment = true;
    // static bool constexpr propagate_on_container_move_assignment = true;
    // static bool constexpr propagate_on_container_swap = true;

    template<class U> struct rebind { using other = HugePageAllocator<U>; };

    using value_type = T;

    HugePageAllocator() noexcept = default;

    template<class U>
    HugePageAllocator(HugePageAllocator<U>) noexcept
    {}

    T* allocate(size_t n) const {
        return static_cast<T*>(hp->allocate(n * sizeof(T)));
    }

    void deallocate(T* p, size_t n) const {
        hp->deallocate(p, n * sizeof(T));
    }

    template<class U>
    bool operator==(HugePageAllocator<U> b) const {
        return hp == b.hp;
    }

    template<class U>
    bool operator!=(HugePageAllocator<U> b) const {
        return hp != b.hp;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // HUGE_PAGES_H_INCLUDED
