/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "cpu_base_frequency.h"

#include <fstream>
#include <stdexcept>
#include <tuple>
#include <regex>
#include <string>

#include <pthread.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double atomic_queue::cpu_base_frequency() {
    std::regex re("model name\\s*:[^@]+@\\s*([0-9.]+)\\s*GHz");
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::smatch m;
    for(std::string line; getline(cpuinfo, line);) {
        regex_match(line, m, re);
        if(m.size() == 2)
            return std::stod(m[1]);
    }
    return 1; // Fallback to cycles, should it fail to parse.
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<atomic_queue::CpuTopologyInfo> atomic_queue::get_cpu_topology_info() {
    std::vector<CpuTopologyInfo> r;
    std::regex res[2] = {
        std::regex("processor\\s+:\\s+([0-9]+)"),
        std::regex("core id\\s+:\\s+([0-9]+)")
    };
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::smatch m;
    unsigned re_idx = 0;
    unsigned values[2];
    for(std::string line; getline(cpuinfo, line);) {
        regex_match(line, m, res[re_idx]);
        if(m.size() != 2)
            continue;
        values[re_idx] = std::stoul(m[1]);
        re_idx ^= 1;
        if(!re_idx)
            r.push_back(CpuTopologyInfo{values[0], values[1]});
    }
    return r;
}

std::vector<atomic_queue::CpuTopologyInfo> atomic_queue::sort_by_core_id(std::vector<atomic_queue::CpuTopologyInfo> const& v) {
    auto u = v;
    std::sort(u.begin(), u.end(), [](auto& a, auto& b) { return std::tie(a.core_id, a.hw_thread_id) < std::tie(b.core_id, b.hw_thread_id); });
    return u;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void atomic_queue::set_thread_affinity(unsigned hw_thread_id) {
    auto thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(hw_thread_id, &cpuset);
    if(int err = ::pthread_setaffinity_np(thread, sizeof cpuset, &cpuset))
        throw std::system_error(err, std::system_category());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
