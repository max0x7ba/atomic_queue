/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "cpu_base_frequency.h"

#include <fstream>
#include <tuple>
#include <regex>
#include <string>
#include <thread>
#include <system_error>

#include <pthread.h>
#include <dlfcn.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double atomic_queue::cpu_base_frequency() {
    std::regex const re("model name\\s*:[^@]+@\\s*([0-9.]+)\\s*GHz");
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::smatch m;
    for(std::string line; getline(cpuinfo, line);)
        if(regex_match(line, m, re))
            return std::stod(m[1]);
    return 1; // Fallback to cycles, should it fail to parse.
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<atomic_queue::CpuTopologyInfo> atomic_queue::get_cpu_topology_info() {
    std::vector<CpuTopologyInfo> cpus;

    unsigned constexpr M = 3;
    using MemberPtr = unsigned CpuTopologyInfo::*;
    MemberPtr const member_ptrs[M] = {
        &CpuTopologyInfo::socket_id,
        &CpuTopologyInfo::core_id,
        &CpuTopologyInfo::hw_thread_id
    };
    std::regex const res[M] = {
        std::regex("physical id\\s+:\\s+([0-9]+)"),
        std::regex("core id\\s+:\\s+([0-9]+)"),
        std::regex("processor\\s+:\\s+([0-9]+)")
    };

    std::ifstream cpuinfo("/proc/cpuinfo");
    std::smatch m;
    CpuTopologyInfo element;
    unsigned valid_members = 0;
    for(std::string line; getline(cpuinfo, line);) {
        for(unsigned i = 0, mask = 1; i < M; ++i, mask <<= 1) {
            if(valid_members & mask || !regex_match(line, m, res[i]))
                continue;
            (element.*member_ptrs[i]) = std::stoul(m[1]);
            valid_members |= mask;
            if(valid_members == ((1u << M) - 1)) {
                cpus.push_back(element);
                valid_members = 0;
            }
            break;
        }
    }

    if(std::thread::hardware_concurrency() != cpus.size())
        throw std::runtime_error("get_cpu_topology_info() invariant broken.");

    return sort_by_hw_thread_id(cpus);
}

std::vector<atomic_queue::CpuTopologyInfo> atomic_queue::sort_by_core_id(std::vector<atomic_queue::CpuTopologyInfo> const& v) {
    auto u = v;
    std::sort(u.begin(), u.end(), [](auto& a, auto& b) {
        return std::tie(a.socket_id, a.core_id, a.hw_thread_id) < std::tie(b.socket_id, b.core_id, b.hw_thread_id);
    });
    return u;
}

std::vector<atomic_queue::CpuTopologyInfo> atomic_queue::sort_by_hw_thread_id(std::vector<atomic_queue::CpuTopologyInfo> const& v) {
    auto u = v;
    std::sort(u.begin(), u.end(), [](auto& a, auto& b) {
        return a.hw_thread_id < b.hw_thread_id;
    });
    return u;
}

std::vector<unsigned> atomic_queue::hw_thread_id(std::vector<atomic_queue::CpuTopologyInfo> const& v) {
    std::vector<unsigned> u(v.size());
    for(unsigned i = 0, j = u.size(); i < j; ++i)
        u[i] = v[i].hw_thread_id;
    return u;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

void set_thread_affinity_(cpu_set_t const& cpuset) {
    // TODO: Investigate whether setting the thread affinity after starting the thread can cause the
    // thread stack to be on a remote NUMA node.
    auto thread = ::pthread_self();
    if(int err = ::pthread_setaffinity_np(thread, sizeof cpuset, &cpuset))
        throw std::system_error(err, std::system_category(), "pthread_setaffinity_np");
}

int default_thread_affinity = -1;
auto const real_pthread_create = reinterpret_cast<decltype(&pthread_create)>(::dlsym(RTLD_NEXT, "pthread_create"));

} // namespace

void atomic_queue::set_thread_affinity(unsigned hw_thread_id) {
    // TODO: Investigate whether setting the thread affinity after starting the thread can cause the
    // thread stack to be on a remote NUMA node.
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(hw_thread_id, &cpuset);
    set_thread_affinity_(cpuset);
}

void atomic_queue::reset_thread_affinity() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for(unsigned i = 0, j = std::thread::hardware_concurrency(); i < j; ++i)
        CPU_SET(i, &cpuset);
    set_thread_affinity_(cpuset);
}

void atomic_queue::set_default_thread_affinity(unsigned hw_thread_id) {
    default_thread_affinity = hw_thread_id;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int pthread_create(pthread_t* newthread,
                   pthread_attr_t const* attr,
                   void*(*start_routine)(void*),
                   void* arg)
{
    if(!real_pthread_create)
        std::abort();

    pthread_attr_t attr2;
    pthread_attr_t *pattr = const_cast<pthread_attr_t*>(attr);
    if(default_thread_affinity >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(default_thread_affinity, &cpuset);
        if(!pattr) {
            if(::pthread_attr_init(&attr2))
                std::abort();
            pattr = &attr2;
        }
        if(::pthread_attr_setaffinity_np(pattr, sizeof cpuset, &cpuset))
            std::abort();
    }

    int r = real_pthread_create(newthread, pattr, start_routine, arg);

    if(pattr == &attr2)
        ::pthread_attr_destroy(&attr2);

    return r;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
