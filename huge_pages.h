/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef HUGE_PAGES_H_INCLUDED
#define HUGE_PAGES_H_INCLUDED

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
    struct Deleter {
        HugePages* hp_;

        template<class T>
        void operator()(T* p) const {
            hp_->destroy(p);
        }
    };

    enum Type { PAGE_2MB = 21, PAGE_1GB = 30 };

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

} // atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // HUGE_PAGES_H_INCLUDED
