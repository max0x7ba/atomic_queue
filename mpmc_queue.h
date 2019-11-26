//
// Lockfree, atomic, multi producer, multi consumer queue
//
// MIT License
//
// Copyright (c) 2019 Erez Strauss, erez@erezstrauss.com
//  http://github.com/erez-strauss/lockfree_mpmc_queue/
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include <atomic>
#include <climits>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <type_traits>

namespace es::lockfree {

__extension__ using uint128_t = unsigned __int128;

template<size_t S>
struct unit_value;
template<>
struct unit_value<8>
{
    using type = uint64_t;
};
template<>
struct unit_value<16>
{
    using type = uint128_t;
};

template<typename T, unsigned A>
class alignas(A) aligned_type : public T
{
};

template<typename T, unsigned N>
class array_inplace
{
    std::array<T, N> _data;
    static_assert(N > 0 && ((N & (N - 1)) == 0),
                  "compile time array requires N to be a power of two: 1, 2, 4, 8, 16 ...");

public:
    explicit array_inplace(size_t = N) noexcept : _data() {}
    ~array_inplace() noexcept = default;
    T& operator[](size_t index) noexcept { return _data[index & (N - 1)]; }
    const T& operator[](size_t index) const noexcept { return _data[index & (N - 1)]; }
    [[nodiscard]] constexpr size_t size() const noexcept { return N; }
    [[nodiscard]] constexpr size_t index_mask() const noexcept { return N - 1; }
    [[nodiscard]] constexpr size_t capacity() const noexcept { return N; }
};

template<typename T, unsigned N>
class array_runtime
{
    std::unique_ptr<T[]> _p;
    const size_t _n;
    const size_t _index_mask;
    static_assert(N == 0, "dynamic, run time array requires N to be zero");

public:
    explicit array_runtime(size_t n = 0) : _p(nullptr), _n(n), _index_mask(n - 1)
    {
        if (_n > 0) _p = std::unique_ptr<T[]>{new T[n]};
    }

    ~array_runtime() noexcept = default;

    T& operator[](size_t index) noexcept { return _p[index & _index_mask]; }
    const T& operator[](size_t index) const noexcept { return _p[index & _index_mask]; }
    [[nodiscard]] size_t size() const noexcept { return _n; }
    [[nodiscard]] size_t index_mask() const noexcept { return _index_mask; }
    [[nodiscard]] size_t capacity() const noexcept { return _n; }
};

template<typename DataT, size_t N = 0, typename IndexT = uint32_t, bool lazy_push = false, bool lazy_pop = false>
class mpmc_queue
{
public:
    using value_type = DataT;
    using index_type = IndexT;

    static_assert(std::is_trivial<value_type>::value, "mpmc_queue requires trivial value_type");
    static_assert(sizeof(index_type) >= 4,
                  "index type should be 4 bytes or wider, one and two bytes index are for experiments only");
    static_assert(std::is_unsigned_v<index_type>, "index_type type must be unsigned");
    static_assert(sizeof(index_type) == 1 || sizeof(index_type) == 2 || sizeof(index_type) == 4 ||
                      sizeof(index_type) == 8,
                  "index_type size must be one of: 1 2 4 8");

    static constexpr size_t get_data_size() { return sizeof(value_type); }
    static constexpr size_t get_index_size() { return sizeof(index_type); }
    static constexpr bool is_lazy_push() { return lazy_push; }
    static constexpr bool is_lazy_pop() { return lazy_pop; }
    static constexpr unsigned bits_in_index() { return sizeof(index_type) * CHAR_BIT; }
    static constexpr unsigned bits_for_value(unsigned n)
    {
        unsigned b{0};
        while (n != 0)
        {
            b++;
            n >>= 1U;
        }
        return b;
    }

    static_assert(N == 0 || (bits_in_index() > bits_for_value(N)), "index type should be wide enough");

private:
    static constexpr bool Q_USE_BUILTIN_16B
    {
#if defined(__GNUC__) && defined(__clang__)
        false  // clang++ uses builtin for std::atomic<__int128>
#elif defined(__GNUC__)
        true  // enable the __sync... builtin functions instead of std::atomic<> that call atomic library.
#else
        false  // other compilers
#endif
    };

    struct alignas(8) helper_entry
    {
        value_type _d;
        index_type _i;
    };
    using entry_as_value = typename unit_value<sizeof(helper_entry)>::type;

    constexpr static inline bool is_always_lock_free = std::atomic<entry_as_value>::is_always_lock_free;

    class alignas(sizeof(helper_entry)) entry
    {
        union entry_union
        {
            mutable entry_as_value _value;
            struct entry_data
            {
                value_type _data;
                index_type _seq;
            } _x;
            entry_union() { _value = 0; }
        } _u;

    public:
        entry() noexcept { clear(); }
        explicit entry(index_type s) noexcept
        {
            clear();
            _u._x._seq = s;
        }
        explicit entry(index_type s, value_type d) noexcept
        {
            clear();
            _u._x._data = d;
            _u._x._seq = s;
        }
        explicit entry(entry_as_value v) noexcept { _u._value = v; }
        ~entry() noexcept = default;

        void clear() noexcept { _u._value = 0; }

        void set_seq(index_type s) noexcept
        {
            clear();
            _u._x._seq = s;
        }

        void set(index_type s, value_type d) noexcept
        {
            clear();
            _u._x._seq = s;
            _u._x._data = d;
        }

        index_type get_seq() noexcept { return _u._x._seq; }

        value_type get_data() noexcept { return _u._x._data; }

        bool is_empty() { return !(_u._x._seq & 1U); }
        bool is_full() { return !is_empty(); }
        bool is_empty() const { return !(_u._x._seq & 1U); }
        bool is_full() const { return !is_empty(); }

        entry& operator=(entry_as_value v) noexcept
        {
            _u._value = v;
            return *this;
        }

        [[using gnu: hot]] entry_as_value load() noexcept
        {
            if constexpr (sizeof(entry_as_value) == 16 && Q_USE_BUILTIN_16B)
                return __sync_val_compare_and_swap(&this->_u._value, 0, 0);
            else
                return reinterpret_cast<std::atomic<entry_as_value>*>(this)->load();
        }

        [[using gnu: hot]] entry_as_value load() const noexcept
        {
            if constexpr (sizeof(entry_as_value) == 16 && Q_USE_BUILTIN_16B)
                return __sync_val_compare_and_swap(&this->_u._value, 0, 0);
            else
                return reinterpret_cast<const std::atomic<entry_as_value>*>(this)->load();
        }

        [[using gnu: hot]] bool compare_exchange(entry expected, entry new_value) noexcept
        {
            if constexpr (sizeof(entry_as_value) == 16 && Q_USE_BUILTIN_16B)
                return __sync_bool_compare_and_swap(&this->_u._value, expected._u._value, new_value._u._value);
            else
                return reinterpret_cast<std::atomic<entry_as_value>*>(this)->compare_exchange_strong(
                    expected._u._value, new_value._u._value);
        }
    };

    static_assert(2 == sizeof(entry) || 4 == sizeof(entry) || 8 == sizeof(entry) || 16 == sizeof(entry),
                  "entry size not supported");
    static_assert(sizeof(entry) == sizeof(helper_entry), "entry and helper_entry are not of the same size");
    static_assert(sizeof(entry) == sizeof(entry_as_value), "entry and entry_as_value are not of the same size");

    inline static constexpr size_t cachelinesize{64};

    using array_t = typename std::conditional<N == 0, array_runtime<aligned_type<entry, 2 * cachelinesize>, 0>,
                                              array_inplace<aligned_type<entry, 2 * cachelinesize>, N>>::type;

public:
    explicit mpmc_queue(uint64_t n = N) : _write_index(0), _read_index(0), _array(n)
    {
        if constexpr (N > 0)
        {
            if (n != N)
                throw(std::invalid_argument{
                    "compile time queue size should be the same as size provided to atomic_mpmc_queue::constructor"});
        }
        else
        {
            if ((n & (n - 1)) != 0 || bits_in_index() <= bits_for_value(n))
                throw(std::invalid_argument{std::string{"wrong size provided to atomic_mpmc_queue constructor: "} +
                                            std::to_string(n)});
        }

        for (index_type i = 0; i < _array.size(); ++i) _array[i].set_seq(i << 1);
    }

    ~mpmc_queue()
    {
        value_type v;
        while (pop(v))
            ;
    }

    mpmc_queue(const mpmc_queue&) = delete;
    mpmc_queue(mpmc_queue&&) = delete;
    mpmc_queue& operator=(const mpmc_queue&) = delete;
    mpmc_queue& operator=(mpmc_queue&&) = delete;

    [[using gnu: hot, flatten]] bool enqueue(value_type d) noexcept { return push(d); }

    [[using gnu: hot, flatten]] bool push(value_type d) noexcept
    {
        index_type wr_index = _write_index.load();

        while (true)
        {
            index_type seq = _array[wr_index].get_seq();

            if (seq == static_cast<index_type>(wr_index << 1))
            {
                entry e{static_cast<index_type>(wr_index << 1)};
                entry data_entry{static_cast<index_type>((wr_index << 1) | 1U), d};

                if (_array[wr_index].compare_exchange(e, data_entry))
                {
                    if constexpr (!lazy_push)
                    {
                        _write_index.compare_exchange_strong(wr_index, wr_index + 1);
                    }
                    return true;
                }
            }
            else if ((seq == static_cast<index_type>((wr_index << 1) | 1U)) ||
                     (static_cast<index_type>(seq) == static_cast<index_type>((wr_index + _array.size()) << 1)))
            {
                _write_index.compare_exchange_strong(wr_index, wr_index + 1);
            }
            else if (static_cast<index_type>(seq + (_array.size() << 1)) ==
                     static_cast<index_type>((wr_index << 1) | 1U))
            {
                return false;
            }

            wr_index = _write_index.load();
        }
    }

    [[using gnu: hot, flatten]] bool dequeue(value_type& d) noexcept { return pop(d); }

    [[using gnu: hot, flatten]] bool pop(value_type& d) noexcept
    {
        index_type rd_index = _read_index.load();

        while (true)
        {
            entry e{_array[rd_index].load()};
            if (e.get_seq() == static_cast<index_type>((rd_index << 1) | 1U))
            {
                entry empty_entry{static_cast<index_type>((rd_index + _array.size()) << 1U)};

                if (_array[rd_index].compare_exchange(e, empty_entry))
                {
                    d = e.get_data();
                    if constexpr (!lazy_pop)
                    {
                        index_type tmp_index = rd_index;
                        ++rd_index;
                        _read_index.compare_exchange_strong(tmp_index, rd_index);
                    }
                    return true;
                }
            }
            else if (static_cast<index_type>(e.get_seq() | 1U) ==
                     static_cast<index_type>(((rd_index + _array.size()) << 1) | 1U))
            {
                index_type tmp_index = rd_index;
                ++rd_index;
                _read_index.compare_exchange_strong(tmp_index, rd_index);
            }
            else if (e.get_seq() == static_cast<index_type>(rd_index << 1))
            {
                return false;
            }
            rd_index = _read_index.load();
        }
    }

    [[using gnu: hot, flatten]] bool enqueue(value_type d, index_type& i) noexcept { return push(d, i); }

    [[using gnu: hot, flatten]] bool push(value_type d, index_type& i) noexcept
    {
        index_type wr_index = _write_index.load();

        while (true)
        {
            index_type seq = _array[wr_index].get_seq();

            if (seq == static_cast<index_type>(wr_index << 1))
            {
                entry e{static_cast<index_type>(wr_index << 1)};
                entry data_entry{static_cast<index_type>((wr_index << 1) | 1U), d};

                if (_array[wr_index].compare_exchange(e, data_entry))
                {
                    i = wr_index;
                    if constexpr (!lazy_push)
                    {
                        _write_index.compare_exchange_strong(wr_index, wr_index + 1);
                    }
                    return true;
                }
            }
            else if ((seq == static_cast<index_type>((wr_index << 1) | 1U)) ||
                     (static_cast<index_type>(seq) == static_cast<index_type>((wr_index + _array.size()) << 1)))
            {
                _write_index.compare_exchange_strong(wr_index, wr_index + 1);
            }
            else if (static_cast<index_type>(seq + (_array.size() << 1)) ==
                     static_cast<index_type>((wr_index << 1) | 1U))
            {
                return false;
            }

            wr_index = _write_index.load();
        }
    }

    [[using gnu: hot, flatten]] bool dequeue(value_type& d, index_type& i) noexcept { return pop(d, i); }

    [[using gnu: hot, flatten]] bool pop(value_type& d, index_type& i) noexcept
    {
        index_type rd_index = _read_index.load();

        while (true)
        {
            entry e{_array[rd_index].load()};
            if (e.get_seq() == static_cast<index_type>((rd_index << 1) | 1U))
            {
                entry empty_entry{static_cast<index_type>((rd_index + _array.size()) << 1U)};

                if (_array[rd_index].compare_exchange(e, empty_entry))
                {
                    d = e.get_data();
                    i = rd_index;
                    if constexpr (!lazy_pop)
                    {
                        index_type tmp_index = rd_index;
                        ++rd_index;
                        _read_index.compare_exchange_strong(tmp_index, rd_index);
                    }
                    return true;
                }
            }
            else if (static_cast<index_type>(e.get_seq() | 1U) ==
                     static_cast<index_type>(((rd_index + _array.size()) << 1) | 1U))
            {
                index_type tmp_index = rd_index;
                ++rd_index;
                _read_index.compare_exchange_strong(tmp_index, rd_index);
            }
            else if (e.get_seq() == static_cast<index_type>(rd_index << 1))
            {
                return false;
            }
            rd_index = _read_index.load();
        }
    }

    [[using gnu: hot, flatten]] bool push_keep_n(value_type d) noexcept
    {
        while (true)
        {
            if (push(d)) return true;
            value_type lost;
            pop(lost);
        }
    }

    [[using gnu: hot, flatten]] bool push_keep_n(value_type d, index_type& i) noexcept
    {
        while (true)
        {
            if (push(d, i)) return true;
            value_type lost;
            pop(lost);
        }
    }

    [[using gnu: hot]] bool peek(value_type& d) noexcept
    {
        index_type rd_index = _read_index.load();

        while (true)
        {
            entry e{_array[rd_index].load()};

            if (e.get_seq() == static_cast<index_type>(rd_index << 1))
            {
                return false;
            }

            if (e.get_seq() == static_cast<index_type>((rd_index << 1) | 1))
            {
                d = e.get_data();
                return true;
            }
            if (static_cast<index_type>(e.get_seq() >> 1) == static_cast<index_type>(rd_index + _array.size()))
            {
                _read_index.compare_exchange_strong(rd_index, rd_index + 1);
            }

            rd_index = _read_index.load();
        }
    }

    [[using gnu: hot]] bool peek(value_type& d, index_type& i) noexcept
    {
        index_type rd_index = _read_index.load();

        while (true)
        {
            entry e{_array[rd_index].load()};

            if (e.get_seq() == static_cast<index_type>(rd_index << 1))
            {
                return false;
            }

            if (e.get_seq() == static_cast<index_type>((rd_index << 1) | 1))
            {
                d = e.get_data();
                i = e.get_seq();
                return true;
            }
            if (static_cast<index_type>(e.get_seq() >> 1) == static_cast<index_type>(rd_index + _array.size()))
            {
                _read_index.compare_exchange_strong(rd_index, rd_index + 1);
            }

            rd_index = _read_index.load();
        }
    }

    template<typename F>
    [[using gnu: hot, flatten]] bool pop_if(F& f, value_type& d) noexcept
    {
        index_type rd_index = _read_index.load();

        while (true)
        {
            entry e{_array[rd_index].load()};

            if (e.get_seq() == static_cast<index_type>(rd_index << 1))
            {
                return false;
            }

            if (e.get_seq() == static_cast<index_type>((rd_index << 1) | 1))
            {
                if (!f(e.get_data(), e.get_seq())) return false;

                entry empty_entry{static_cast<index_type>((rd_index + _array.size()) << 1)};

                if (_array[rd_index].compare_exchange(e, empty_entry))
                {
                    d = e.get_data();
                    if constexpr (!lazy_pop)
                    {
                        index_type tmp_index = rd_index;
                        _read_index.compare_exchange_strong(tmp_index, rd_index + 1);
                    }
                    return true;
                }
            }
            else if (static_cast<index_type>(e.get_seq() >> 1) == static_cast<index_type>(rd_index + _array.size()))
            {
                _read_index.compare_exchange_strong(rd_index, rd_index + 1);
            }

            rd_index = _read_index.load();
        }
    }

    template<typename F>
    [[using gnu: hot, flatten]] uint64_t consume(F&& f)
    {
        uint64_t r{0};
        value_type v;
        index_type i;
        while (pop(v, i))
        {
            ++r;
            if (f(v, i)) return r;
        }
        return r;
    }

    [[using gnu: hot, flatten]] bool empty() noexcept
    {
        index_type rd_index = _read_index.load();
        entry e{_array[rd_index].load()};

        if (e.get_seq() == static_cast<index_type>(rd_index << 1)) return true;
        return false;
    }
    [[using gnu: hot, flatten]] [[nodiscard]] bool empty() const noexcept
    {
        index_type rd_index = _read_index.load();
        entry e{_array[rd_index].load()};

        if (e.get_seq() == static_cast<index_type>(rd_index << 1)) return true;
        return false;
    }

    [[using gnu: hot, flatten]] [[nodiscard]] size_t size() const noexcept
    {
        if (empty()) return 0;
        if (_write_index >= _read_index) return _write_index - _read_index;
        return _write_index + _array.size() - _read_index;
    }

    [[nodiscard]] size_t capacity() const noexcept { return _array.size(); }

    [[nodiscard]] constexpr size_t entry_size() const noexcept { return sizeof(entry); }
    [[nodiscard]] static constexpr size_t size_n() { return N; }

    [[using gnu: used]] std::ostream& dump_state(std::ostream& os) noexcept;

    [[using gnu: used, noinline, weak]] void dump_state() noexcept;

private:
    std::atomic<index_type> _write_index alignas(2 * cachelinesize);
    std::atomic<index_type> _read_index alignas(2 * cachelinesize);
    array_t _array;
};

template<typename Data_t, size_t N, typename Index_t, bool lazy_push, bool lazy_pop>
[[gnu::weak, gnu::noinline, gnu::used]] void mpmc_queue<Data_t, N, Index_t, lazy_push, lazy_pop>::dump_state() noexcept
{
    dump_state(std::cerr);
}

template<typename DataT, size_t N, typename IndexT, bool lazy_push, bool lazy_pop>
inline std::ostream& mpmc_queue<DataT, N, IndexT, lazy_push, lazy_pop>::dump_state(std::ostream& os) noexcept
{
    auto show_entry = [&](std::ostream& eos, unsigned eindex) -> std::ostream& {
        entry e(static_cast<entry_as_value>(_array[eindex].load()));
        uint64_t seq_index = (e.get_seq() >> 1);

        eos << "  e[" << std::setw(3) << eindex << "]:\t " << seq_index << "\t "
            << ((uint64_t)(e.get_seq() >> 1) / _array.size()) << '_' << ((uint64_t)(e.get_seq() >> 1) % _array.size())
            << "\t" << ((e.get_seq() & 0x01) ? "Full " : "Empty") << " d: " << e.get_data();
        return eos;
    };
    unsigned data_entries_count{0};
    for (unsigned i = 0; i < _array.size(); ++i)
    {
        entry& e{_array[i]};
        data_entries_count += (e.get_seq() & 1);
    }
    index_type ewr_index = _write_index.load() & ~(1UL << (bits_in_index() - 1));
    index_type erd_index = _read_index.load() & ~(1UL << (bits_in_index() - 1));
    index_type indexdiff = (ewr_index >= erd_index) ? (ewr_index - erd_index) : (ewr_index + _array.size() - erd_index);
    // clang-format off
    os << "mpmc_queue Queue dump:"
       << "\n Q is_always_lock_free: " << (is_always_lock_free ? "true" : "false")
       << "\n Q Capacity: " << (uint64_t)_array.size() << (N==0?" runtime":" compile time")
       << "\n Q Size: " << size()
       << "\n Q lazy push: " << (is_lazy_push()?"true":"false")
       << "\n Q lazy pop: " << (is_lazy_pop()?"true":"false")
       << "\n Q sizeof(*this): " << sizeof(*this)
       << "\n Q Entries with data: " << data_entries_count
       << "\n Q entry size: " << entry_size()
       << "\n Q index_mask: 0x" << std::hex << (uint64_t)(_array.size() - 1) << std::dec
       << "\n Q effective write index: " << (uint64_t)ewr_index << " " << (ewr_index/_array.size()) << '_' << ( ewr_index%_array.size())
       << "\n Q effective read index: " << (uint64_t)erd_index << " " << (erd_index/_array.size()) << '_' << (erd_index%_array.size())
       << "\n Q write index: " << (uint64_t)_write_index << " " << (_write_index / _array.size()) << '_' << (_write_index % _array.size()) << "  -->";
    show_entry(os, (unsigned)(_write_index&_array.index_mask()))
      << "\n Q read index: " << (uint64_t)_read_index << " " << (_read_index / _array.size()) << '_' << (_read_index % _array.size()) << "  -->";
    show_entry(os, (unsigned)(_read_index&_array.index_mask()))
       << "\n Q write-read indexes: " << (unsigned long) indexdiff << '\n';
    // clang-format on
    for (unsigned i = 0; i < _array.size(); ++i)
    {
        entry e(static_cast<entry_as_value>(_array[i].load()));
        show_entry(os, i) << '\n';
    }
    os << '\n';
    return os;
}

template<typename Data_t, size_t N, typename Index_t, bool lazy_push, bool lazy_pop>
inline std::ostream& operator<<(std::ostream& os, mpmc_queue<Data_t, N, Index_t, lazy_push, lazy_pop>& q) noexcept
{
    return q.dump_state(os);
}

}  // namespace es::lockfree
