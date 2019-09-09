/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#include "huge_pages.h"

#include <cstdio>
#include <system_error>

#include <sys/mman.h>

#ifndef MAP_HUGE_SHIFT
#define MAP_HUGE_SHIFT 26
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool hugeadm_warn_1GB = true;
bool hugeadm_warn_2MB = true;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

atomic_queue::HugePages::HugePages(Type t, size_t size) {
    void* p;
    size_t total_size;
    for(;;) {
        unsigned flags;
        size_t page_size;
        if(t != PAGE_DEFAULT) {
            page_size = 1u << t;
            flags = (t << MAP_HUGE_SHIFT) | MAP_HUGETLB;
        }
        else {
            page_size = 4096;
            flags = 0;
        }
        total_size = (size + (page_size - 1)) & ~(page_size - 1); // Round up to the page size.
        p = ::mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED | flags, -1, 0);
        if(p != MAP_FAILED)
            break;

        // Try using smaller huge pages.
        if(t == PAGE_1GB) {
            t = PAGE_2MB;
            if(hugeadm_warn_1GB) {
                hugeadm_warn_1GB = false;
                std::fprintf(stderr, "Warning: Failed to allocate 1GB huge pages. Run \"sudo hugeadm --pool-pages-min 1GB:1 --pool-pages-max 1GB:1\".\n");
            }
        }
        else if(t == PAGE_2MB) {
            t = PAGE_DEFAULT;
            if(hugeadm_warn_2MB) {
                hugeadm_warn_2MB = false;
                std::fprintf(stderr, "Warning: Failed to allocate 2MB huge pages. Run \"sudo hugeadm --pool-pages-min 2MB:16 --pool-pages-max 2MB:16\".\n");
            }
        }
        else
            throw std::system_error(errno, std::system_category(), "mmap");
    }

    beg_ = static_cast<unsigned char*>(p);
    cur_ = beg_;
    end_ = beg_ + total_size;
}

atomic_queue::HugePages::~HugePages() {
    if(beg_)
        ::munmap(beg_, end_ - beg_);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

atomic_queue::HugePages* atomic_queue::HugePageAllocatorBase::hp = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
