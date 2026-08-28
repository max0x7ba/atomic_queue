/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "cpu_base_frequency.h"
#include "atomic_queue/defs.h"

#include <stdexcept>
#include <fstream>
#include <tuple>
#include <regex>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <string>
#include <thread>
#include <system_error>
#include <unordered_map>

#include <pthread.h>
#include <dlfcn.h>
#include <sched.h>
#include <strings.h>

using namespace atomic_queue;
using std::printf;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CpuSet : cpu_set_t {
    CpuSet() noexcept { CPU_ZERO(this); }
    bool is_set(int cpu) const noexcept { return CPU_ISSET(cpu, this); }
    unsigned size() const noexcept { return CPU_COUNT(this); }

    static CpuSet get_available() {
        CpuSet cpus;
        cpu_set_t& c = cpus;
        if(int err = ::pthread_getaffinity_np(::pthread_self(), sizeof c, &c))
            throw std::system_error(err, std::system_category(), "pthread_getaffinity_np");
        return cpus;
    }

    void filter(std::vector<CoreInfo>& c) const {
        c.erase(
            std::remove_if(c.begin(), c.end(), [p = c.data(), this](auto& cpu_info) { return !is_set(&cpu_info - p); }),
            c.end()
            );
        if(size() != c.size())
            throw std::runtime_error("CpuSet::filter invariant broken.");
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double const MIPS_TO_GIPS = 1e3;

inline double bogomips_to_ghz(double bogomips, double n_cpu_cores, double n_siblings) noexcept {
    auto n_threads_per_cpu_core = n_siblings / n_cpu_cores; // 2 for SMT, 1 otherwise.
    return bogomips / (MIPS_TO_GIPS * n_threads_per_cpu_core);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CpuInfoParser {
    // The key-value separator is colon-space (: ).
    // Strip any whitespace off keys and values.
    std::regex const re_kv{"\\s*([^:]+?)\\s*:\\s+(.+?)\\s*"};

    std::ifstream cpuinfo{"/proc/cpuinfo"};

    std::string line;
    std::cmatch kv;

    auto& next() { return getline(cpuinfo, line); }
    auto parse() { return regex_match(line.c_str(), kv, re_kv); }

    template<size_t N>
    bool operator==(char const(&key)[N]) const noexcept {
        auto& k = kv[1];
        auto n = k.length();
        return n == N - 1 && !::strncasecmp(k.first, key, n); // BogoMIPS and bogomips are the same thing.
    }

    operator std::string() const { return kv[2]; }
    operator unsigned() const { return std::stoul(kv[2]); }
    operator double() const { return std::stod(kv[2]); }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CpuInfo::CpuInfo() {
    double bogomips = MIPS_TO_GIPS, n_cpu_cores = bogomips, n_siblings = bogomips;

    for(CpuInfoParser kv; kv.next();) {
        if(!kv.parse())
            continue;

        if(kv == "processor")
            cores.push_back({kv, 0, 0});
        else if(kv == "physical id")
            cores.back().socket_id = kv;
        else if(kv == "core id")
            cores.back().core_id = kv;

        // These come from CPU#0 only.
        // Doesn't handle non-homogeneous CPUs (e.g. ARM big.LITTLE).
        else if(cores.size() <= 1) {
            if(kv == "model name")
                model_name.assign(kv);
            else if(kv == "bogomips")
                bogomips = kv;
            else if(kv == "cpu cores")
                n_cpu_cores = kv;
            else if(kv == "siblings")
                n_siblings = kv;
        }
    }

    ghz = bogomips_to_ghz(bogomips, n_cpu_cores, n_siblings);

    if(std::thread::hardware_concurrency() != cores.size())
        throw std::runtime_error("CpuInfo::cores invariant broken.");
    CpuSet::get_available().filter(cores);
}

void CpuInfo::log(char const* clock_name) const {
    if(!model_name.empty())
        printf("CPU Model: %s, ", model_name.c_str());

    printf("base GHz: %.1lf, clock: %s, ", ghz, clock_name);

    printf("%zu available CPUs: ", cores.size());
    char sep = '[';
    for(auto& cpu : cores) {
        printf("%c%u", sep, cpu.hw_thread_id);
        sep = ',';
    }
    printf("].\n");
}

std::vector<unsigned> CpuInfo::hw_thread_ids() const {
    std::vector<unsigned> u(cores.size());
    for(unsigned i = 0, j = u.size(); i < j; ++i)
        u[i] = cores[i].hw_thread_id;
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

// void atomic_queue::reset_thread_affinity() {
//     cpu_set_t cpuset;
//     CPU_ZERO(&cpuset);
//     for(unsigned i = 0, j = std::thread::hardware_concurrency(); i < j; ++i)
//         CPU_SET(i, &cpuset);
//     set_thread_affinity_(cpuset);
// }

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

    cpu_set_t cpuset;
    pthread_attr_t attr2;
    pthread_attr_t *pattr = const_cast<pthread_attr_t*>(attr);

    if(default_thread_affinity >= 0) {
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

EnvBits64::EnvBits64(char const* env_name, U default_bits)
    : value{default_bits}
{
    if(char const* value_beg = std::getenv(env_name)) {
        // strtoull auto-detects base-8, base-10, base-16 from the prefix. But not base-2.
        // Enable parsing base-2 values with 0b prefix.
        int const base2 = 2 * (value_beg[0] == '0' && value_beg[1] == 'b');
        value_beg += base2;

        char* value_end = 0;
        value = std::strtoull(value_beg, &value_end, base2);
        if(!value_end || *value_end || (ULLONG_MAX == value && ERANGE == errno))
            throw std::out_of_range(env_name);
    }
}

EnvBits64::EnvBits64(char const* env_name, U default_bits, U min, U max)
    : EnvBits64(env_name, default_bits)
{
    if(value < min || value > max)
        throw std::out_of_range(env_name);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
