/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#include "huge_pages.h"

#include <system_error>

#include <unistd.h>
#include <sys/mman.h>

#ifndef MAP_HUGE_SHIFT
#define MAP_HUGE_SHIFT 26
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t const default_page_size = ::sysconf(_SC_PAGESIZE);

atomic_queue::HugePages::WarnFn* atomic_queue::HugePages::warn_no_1GB_pages = 0;
atomic_queue::HugePages::WarnFn* atomic_queue::HugePages::warn_no_2MB_pages = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

atomic_queue::HugePages::HugePages(Type t, size_t size) {
    void* p;
    size_t total_size;

    for(;;) {
        unsigned flags = 0;
        size_t page_size = default_page_size;
        if(t != PAGE_DEFAULT) {
            page_size = 1u << t;
            flags = (t << MAP_HUGE_SHIFT) | MAP_HUGETLB;
        }
        total_size = (size + (page_size - 1)) & ~(page_size - 1); // Round up to the page size.

        p = ::mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED | flags, -1, 0);
        if(p != MAP_FAILED)
            break;

        // Try using smaller page sizes.
        if(t == PAGE_1GB) {
            t = PAGE_2MB;
            if(warn_no_1GB_pages) {
                warn_no_1GB_pages();
                warn_no_1GB_pages = 0; // Warn once.
            }
        }
        else if(t == PAGE_2MB) {
            t = PAGE_DEFAULT;
            if(warn_no_2MB_pages) {
                warn_no_2MB_pages();
                warn_no_2MB_pages = 0; // Warn once.
            }
        }
        else
            throw std::system_error(errno, std::system_category(), "mmap");
    }

    beg_ = static_cast<unsigned char*>(p);
    cur_ = beg_;
    end_ = beg_ + total_size;
}

atomic_queue::HugePages::~HugePages() noexcept {
    if(beg_)
        ::munmap(beg_, end_ - beg_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

atomic_queue::HugePages* atomic_queue::HugePageAllocatorBase::hp = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
