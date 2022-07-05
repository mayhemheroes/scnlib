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

#ifndef SCN_DETAIL_ERASED_RANGE_IMPL_H
#define SCN_DETAIL_ERASED_RANGE_IMPL_H

#include "erased_range.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    namespace detail {
        template <typename CharT>
        expected<CharT> wrap_in_expected(expected<CharT> v)
        {
            return v;
        }
        inline expected<char> wrap_in_expected(char ch)
        {
            return {ch};
        }
        inline expected<wchar_t> wrap_in_expected(wchar_t ch)
        {
            return {ch};
        }

        template <typename StorageType, typename Range, typename CharT>
        template <typename R>
        std::ptrdiff_t
        basic_erased_range_impl<StorageType, Range, CharT>::_fill_buffer_impl(
            std::true_type)
        {
            auto s = get_buffer(m_range.get(), m_next_to_read_from_source);
            if (s.size() > 0) {
                auto old_size = m_buffer.size();
                m_buffer.resize(m_buffer.size() + s.size());
                std::copy(s.begin(), s.end(), &m_buffer[old_size]);

                ranges::advance(m_next_to_read_from_source, s.ssize());
            }
            return s.ssize();
        }
        template <typename StorageType, typename Range, typename CharT>
        template <typename R>
        std::ptrdiff_t
        basic_erased_range_impl<StorageType, Range, CharT>::_fill_buffer_impl(
            std::false_type)
        {
            return 0;
        }

        template <typename StorageType, typename Range, typename CharT>
        std::ptrdiff_t
        basic_erased_range_impl<StorageType, Range, CharT>::_fill_buffer()
        {
            return _fill_buffer_impl<range_type>(
                std::integral_constant<bool, provides_buffer_access_impl<
                                                 range_nocvref_type>::value>{});
        }

        template <typename StorageType, typename Range, typename CharT>
        error
        basic_erased_range_impl<StorageType, Range, CharT>::_read_until_index(
            std::ptrdiff_t i)
        {
            auto chars_to_read = i - m_next_char_buffer_index + 1;

            const auto chars_read_into_buffer = _fill_buffer();
            if (chars_read_into_buffer >= chars_to_read) {
                return {};
            }

            chars_to_read -= chars_read_into_buffer;
            m_buffer.reserve(m_buffer.size() +
                             static_cast<size_t>(chars_to_read));
            for (; chars_to_read > 0; --chars_to_read) {
                if (m_next_to_read_from_source == ranges::end(m_range.get())) {
                    return {error::end_of_range, "EOF"};
                }

                auto ret = wrap_in_expected(*m_next_to_read_from_source);
                if (!ret) {
                    return ret.error();
                }

                m_buffer.push_back(ret.value());
                ++m_next_to_read_from_source;
            }

            return {};
        }

        template <typename StorageType, typename Range, typename CharT>
        auto basic_erased_range_impl<StorageType, Range, CharT>::do_get_at(
            std::ptrdiff_t i) -> expected<char_type>
        {
            if (i >= static_cast<std::ptrdiff_t>(m_buffer.size())) {
                auto e = _read_until_index(i);
                if (!e) {
                    return e;
                }
            }

            if (i >= m_next_char_buffer_index) {
                m_next_char_buffer_index = i + 1;
            }
            return m_buffer[static_cast<size_t>(i)];
        }

        template <typename StorageType, typename Range, typename CharT>
        auto basic_erased_range_impl<StorageType, Range, CharT>::
            do_avail_starting_at(std::ptrdiff_t i) const
            -> span<const char_type>
        {
            if (i >= static_cast<std::ptrdiff_t>(m_buffer.size())) {
                return {};
            }
            return {m_buffer.data() + i, m_buffer.data() + m_buffer.size()};
        }

        template <typename StorageType, typename Range, typename CharT>
        std::ptrdiff_t
        basic_erased_range_impl<StorageType, Range, CharT>::do_current_index()
            const
        {
            return m_next_char_buffer_index;
        }
        template <typename StorageType, typename Range, typename CharT>
        bool
        basic_erased_range_impl<StorageType, Range, CharT>::do_is_index_at_end(
            std::ptrdiff_t i) const
        {
            if (i < do_current_index()) {
                return false;
            }
            return m_next_to_read_from_source == ranges::end(m_range.get()) &&
                   m_next_char_buffer_index ==
                       static_cast<std::ptrdiff_t>(m_buffer.size());
        }

        template <typename StorageType, typename Range, typename CharT>
        error
        basic_erased_range_impl<StorageType, Range, CharT>::do_advance_current(
            std::ptrdiff_t n)
        {
            ranges::advance(m_next_to_read_from_source, n);
            return {};
        }

    }  // namespace detail

    template <typename CharT>
    class basic_erased_range<CharT>::iterator {
        friend class basic_erased_range<CharT>;

        struct arrow_access_proxy {
            expected<CharT> val;

            const expected<CharT>* operator->() const
            {
                return &val;
            }
        };

    public:
        using value_type = expected<CharT>;
        using reference = expected<CharT>&;
        using pointer = expected<CharT>*;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::bidirectional_iterator_tag;
        using range_type = basic_erased_range<CharT>;

        SCN_CONSTEXPR14 iterator() noexcept = default;

        expected<CharT> operator*() const
        {
            SCN_EXPECT(m_range);
            return m_range->m_impl->get_at(m_index);
        }
        arrow_access_proxy operator->() const
        {
            SCN_EXPECT(m_range);
            return {**this};
        }

        iterator& operator++()
        {
            SCN_EXPECT(m_range);
            ++m_index;
            m_range->m_impl->get_at(m_index);
            return *this;
        }
        iterator operator++(int)
        {
            auto tmp = *this;
            operator++();
            return tmp;
        }

        iterator& operator--()
        {
            SCN_EXPECT(m_range);
            SCN_EXPECT(m_index > 0);
            --m_index;
            return *this;
        }
        iterator operator--(int)
        {
            auto tmp = *this;
            operator--();
            return tmp;
        }

        bool operator==(const iterator& other) const
        {
            if (_is_sentinel() && other._is_sentinel()) {
                return true;
            }
            if (_is_sentinel() || other._is_sentinel()) {
                return false;
            }
            return m_index == other.m_index;
        }
        bool operator!=(const iterator& other) const
        {
            return !operator==(other);
        }

        bool operator<(const iterator& other) const
        {
            if (_is_sentinel() && other._is_sentinel()) {
                return false;
            }
            if (_is_sentinel()) {
                return false;
            }
            if (other._is_sentinel()) {
                return true;
            }
            return m_index < other.m_index;
        }
        bool operator>(const iterator& other) const
        {
            return other.operator<(*this);
        }
        bool operator<=(const iterator& other) const
        {
            return !operator>(other);
        }
        bool operator>=(const iterator& other) const
        {
            return !operator<(other);
        }

        basic_erased_range<CharT>* get_range() noexcept
        {
            return m_range;
        }
        const basic_erased_range<CharT>* get_range() const noexcept
        {
            return m_range;
        }

    private:
        iterator(const basic_erased_range<CharT>& r, std::ptrdiff_t i) noexcept
            : m_range(const_cast<basic_erased_range<CharT>*>(&r)), m_index(i)
        {
        }

        bool _is_sentinel() const
        {
            if (!m_range || !m_range->m_impl) {
                return true;
            }
            SCN_EXPECT(m_range->m_impl);
            return m_range->m_impl->is_index_at_end(m_index);
        }

        mutable basic_erased_range<CharT>* m_range{nullptr};
        mutable std::ptrdiff_t m_index{0};
    };

    template <typename CharT>
    basic_erased_range<CharT>::basic_erased_range(iterator b, sentinel)
    {
        if (!b.m_range) {
            return;
        }

        auto&& range = SCN_MOVE(*b.m_range);
        m_impl = SCN_MOVE(range.m_impl);
        m_begin_index = b.m_index;
    }

    template <typename CharT>
    auto basic_erased_range<CharT>::begin() const noexcept -> iterator
    {
        return iterator{*this, m_begin_index};
    }
    template <typename CharT>
    auto basic_erased_range<CharT>::end() const noexcept -> sentinel
    {
        return {};
    }

    template <typename CharT>
    auto basic_erased_range<CharT>::get_buffer(iterator b,
                                               std::size_t max_size) const
        -> span<const char_type>
    {
        SCN_EXPECT(m_impl);
        auto s = m_impl->avail_starting_at(b.m_index);
        if (s.size() > max_size) {
            return s.first(max_size);
        }
        return s;
    }

    SCN_END_NAMESPACE
}  // namespace scn

#endif
