// Copyright 2017 Elias Kosunen
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file is a part of scnlib:
//     https://github.com/eliaskosunen/scnlib

#ifndef SCN_DETAIL_WRAPPER_H
#define SCN_DETAIL_WRAPPER_H

#include "../ranges/ranges.h"
#include "../util/memory.h"
#include "prepare.h"
#include "vectored.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    template <typename CharT>
    class basic_string_view_wrapper {
    public:
        using char_type = CharT;
        using range_type = basic_string_view<CharT>;
        using iterator = typename range_type::const_iterator;
        using sentinel = typename range_type::const_iterator;
        using difference_type = typename range_type::difference_type;
        using value_type = const CharT;
        using pointer = const CharT*;

        SCN_CONSTEXPR14 basic_string_view_wrapper(range_type r) noexcept
            : m_range(SCN_MOVE(r)), m_begin(m_range.begin())
        {
        }

        basic_string_view_wrapper& operator=(range_type other)
        {
            m_range = other;
            m_begin = m_range.begin();
            m_read = 0;
            return *this;
        }

        constexpr iterator begin() const noexcept
        {
            return m_begin;
        }
        constexpr sentinel end() const noexcept
        {
            return m_range.end();
        }

        constexpr bool empty() const noexcept
        {
            return begin() == end();
        }

        SCN_CONSTEXPR14 iterator advance(difference_type n = 1) noexcept
        {
            SCN_EXPECT(m_begin + n <= end());
            m_read += n;
            m_begin += n;
            return m_begin;
        }

        SCN_CONSTEXPR14 void advance_to(iterator it) noexcept
        {
            const auto diff = it - m_begin;
            m_read += diff;
            m_begin = it;
        }

        SCN_CONSTEXPR14 iterator begin_underlying() const noexcept
        {
            return m_range.begin();
        }

        SCN_CONSTEXPR14 range_type range_underlying() const noexcept
        {
            return m_range;
        }

        SCN_CONSTEXPR14 pointer data() const noexcept
        {
            return detail::to_address(m_begin);
        }
        constexpr difference_type size() const noexcept
        {
            return end() - m_begin;
        }

        span<const char_type> get_buffer_and_advance(
            std::size_t max_size = std::numeric_limits<size_t>::max())
        {
            auto buf = detail::get_buffer(m_range, begin(), max_size);
            advance(buf.ssize());
            return buf;
        }

        SCN_CONSTEXPR14 void reset_to_rollback_point() noexcept
        {
            SCN_EXPECT(m_begin - m_range.begin() >= m_read);
            m_begin -= m_read;
            m_read = 0;
        }
        SCN_CONSTEXPR14 void set_rollback_point() const noexcept
        {
            m_read = 0;
        }

        range_type reconstructed()
        {
            return {begin(), end()};
        }

        ready_prepared_range<basic_string_view<CharT>> prepare() const noexcept
        {
            return {{data(), static_cast<std::size_t>(size())}};
        }

        static constexpr bool is_direct = true;
        static constexpr bool is_contiguous = true;
        static constexpr bool provides_buffer_access = true;

    private:
        range_type m_range;
        iterator m_begin;
        mutable difference_type m_read{0};
    };

    using string_view_wrapper = basic_string_view_wrapper<char>;
    using wstring_view_wrapper = basic_string_view_wrapper<wchar_t>;

    template <typename CharT>
    class basic_erased_view_wrapper {
    public:
        using char_type = CharT;
        using range_type = basic_erased_view<CharT>;
        using iterator = typename range_type::iterator;
        using sentinel = typename range_type::sentinel;
        using difference_type = std::ptrdiff_t;
        using value_type = expected<CharT>;

        basic_erased_view_wrapper(range_type r)
            : m_range(SCN_MOVE(r)), m_begin(m_range.begin())
        {
        }

        basic_erased_view_wrapper& operator=(range_type other)
        {
            m_range = other;
            m_begin = m_range.begin();
            m_read = 0;
            return *this;
        }

        iterator begin() const noexcept
        {
            return m_begin;
        }
        sentinel end() const noexcept
        {
            return m_range.end();
        }

        bool empty() const
        {
            return begin() == end();
        }

        iterator advance(difference_type n = 1)
        {
            m_read += n;
            ranges::advance(m_begin, n);
            return m_begin;
        }

        void advance_to(iterator it)
        {
            while (m_begin != it) {
                ++m_read;
                ++m_begin;
            }
        }

        iterator begin_underlying() const
        {
            return m_range.begin();
        }

        range_type range_underlying() const noexcept
        {
            return m_range;
        }

        span<const char_type> get_buffer_and_advance(
            std::size_t max_size = std::numeric_limits<size_t>::max())
        {
            auto buf = detail::get_buffer(m_range, begin(), max_size);
            if (buf.size() != 0) {
                advance(buf.ssize());
            }
            return buf;
        }

        void reset_to_rollback_point()
        {
            ranges::advance(m_begin, -m_read);
            set_rollback_point();
        }
        void set_rollback_point()
        {
            m_read = 0;
        }

        range_type reconstructed()
        {
            return {begin(), end()};
        }

        ready_prepared_range<basic_erased_view<CharT>> prepare() const noexcept
        {
            return {{begin(), end()}};
        }

        static constexpr bool is_direct = false;
        static constexpr bool is_contiguous = false;
        static constexpr bool provides_buffer_access = true;

    private:
        range_type m_range;
        iterator m_begin;
        mutable difference_type m_read{0};
    };

    using erased_view_wrapper = basic_erased_view_wrapper<char>;
    using werased_view_wrapper = basic_erased_view_wrapper<wchar_t>;

    template <typename CharT>
    basic_string_view_wrapper<CharT> wrap(basic_string_view<CharT> s)
    {
        return {s};
    }
    template <typename CharT>
    basic_erased_view_wrapper<CharT> wrap(basic_erased_view<CharT> e)
    {
        return {e};
    }
    template <typename CharT>
    basic_string_view_wrapper<CharT> wrap(basic_string_view_wrapper<CharT> w)
    {
        return w;
    }
    template <typename CharT>
    basic_erased_view_wrapper<CharT> wrap(basic_erased_view_wrapper<CharT> w)
    {
        return w;
    }

    SCN_END_NAMESPACE
}  // namespace scn

#endif
