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

#ifndef SCN_DETAIL_ERASED_RANGE_H
#define SCN_DETAIL_ERASED_RANGE_H

#include "../ranges/ranges.h"
#include "../util/expected.h"
#include "../util/unique_ptr.h"
#include "vectored.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    namespace detail {
        template <typename Iterator, typename = void>
        struct extract_char_type;
        template <typename Iterator>
        struct extract_char_type<
            Iterator,
            typename std::enable_if<std::is_integral<
                polyfill_2a::iter_value_t<Iterator>>::value>::type> {
            using type = polyfill_2a::iter_value_t<Iterator>;
        };
        template <typename Iterator>
        struct extract_char_type<
            Iterator,
            void_t<
                typename std::enable_if<!std::is_integral<
                    polyfill_2a::iter_value_t<Iterator>>::value>::type,
                typename polyfill_2a::iter_value_t<Iterator>::success_type>> {
            using type =
                typename polyfill_2a::iter_value_t<Iterator>::success_type;
        };

        template <typename CharT>
        class basic_erased_range_impl_base {
        public:
            basic_erased_range_impl_base() = default;

            basic_erased_range_impl_base(const basic_erased_range_impl_base&) =
                delete;
            basic_erased_range_impl_base& operator=(
                const basic_erased_range_impl_base&) = delete;

            basic_erased_range_impl_base(basic_erased_range_impl_base&&) =
                default;
            basic_erased_range_impl_base& operator=(
                basic_erased_range_impl_base&&) = default;

            virtual ~basic_erased_range_impl_base() = default;

            // char at position
            expected<CharT> get_at(std::ptrdiff_t i)
            {
                SCN_EXPECT(i >= 0);
                return do_get_at(i);
            }
            // ready chars starting from position
            span<const CharT> avail_starting_at(std::ptrdiff_t i) const
            {
                SCN_EXPECT(i >= 0);
                return do_avail_starting_at(i);
            }

            std::ptrdiff_t current_index() const
            {
                return do_current_index();
            }
            bool is_current_at_end() const
            {
                return do_is_current_at_end();
            }

            error advance_current(std::ptrdiff_t n)
            {
                return do_advance_current(n);
            }

        protected:
            // char at position
            virtual expected<CharT> do_get_at(std::ptrdiff_t) = 0;
            // ready chars starting from position
            virtual span<const CharT> do_avail_starting_at(
                std::ptrdiff_t) const = 0;

            virtual std::ptrdiff_t do_current_index() const = 0;
            virtual bool do_is_current_at_end() const = 0;

            virtual error do_advance_current(std::ptrdiff_t) = 0;
        };

        template <typename T>
        using _erased_range_impl_marker = typename T::erased_range_impl_marker;

        template <typename T>
        struct _has_erased_range_impl_marker
            : custom_ranges::detail::exists<_erased_range_impl_marker, T> {
        };

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

        template <typename Range, typename CharT>
        class basic_erased_range_impl
            : public basic_erased_range_impl_base<CharT> {
            using base = basic_erased_range_impl_base<CharT>;

        public:
            using char_type = CharT;
            using range_type = Range;
            using range_nocvref_type = remove_cvref_t<range_type>;
            using iterator = ranges::iterator_t<range_type>;
            using sentinel = ranges::sentinel_t<range_type>;

            using erased_range_impl_marker = void;

            template <typename R = range_type,
                      typename std::enable_if<!_has_erased_range_impl_marker<
                          remove_cvref_t<R>>::value>::type* = nullptr>
            basic_erased_range_impl(R&& r)
                : m_range{SCN_FWD(r)}, m_current{ranges::begin(m_range)}
            {
            }

            basic_erased_range_impl(const basic_erased_range_impl&) = delete;
            basic_erased_range_impl& operator=(const basic_erased_range_impl&) =
                delete;

            basic_erased_range_impl(basic_erased_range_impl&& o)
                : m_range{SCN_MOVE(o.m_range)},
                  m_buffer{SCN_MOVE(o.m_buffer)},
                  m_current{ranges::begin(m_range)},
                  m_read{o.m_read}
            {
                auto e = base::advance_current(o.m_read);
                SCN_ENSURE(e);
            }
            basic_erased_range_impl& operator=(basic_erased_range_impl&&) =
                delete;

            ~basic_erased_range_impl() override = default;

        private:
            template <typename R>
            void _fill_buffer_impl(std::true_type)
            {
                auto s = get_buffer(m_range, m_current);
                if (s.size() > 0) {
                    auto old_size = m_buffer.size();
                    m_buffer.resize(m_buffer.size() + s.size());
                    std::copy(s.begin(), s.end(), &m_buffer[old_size]);
                    base::advance_current(s.ssize());
                }
            }
            template <typename R>
            void _fill_buffer_impl(std::false_type)
            {
            }
            void _fill_buffer()
            {
                return _fill_buffer_impl<range_type>(
                    std::integral_constant<bool,
                                           provides_buffer_access_impl<
                                               range_nocvref_type>::value>{});
            }

            // read until char index i
            error _read_until_index(std::ptrdiff_t i)
            {
                _fill_buffer();

                // _get_index_in_buffer(i) - m_buffer.size()
                //  -> (i - (m_read - m_buffer.size()) - m_buffer.size()
                //  -> i - m_read + m_buffer.size() - m_buffer.size()
                //  -> i - m_read
                auto chars_to_read = i - m_read;
                if (chars_to_read <= 0) {
                    if (i >= static_cast<std::ptrdiff_t>(m_buffer.size())) {
                        return {error::end_of_range, "EOF"};
                    }
                    return {};
                }

                m_buffer.reserve(m_buffer.size() +
                                 static_cast<size_t>(chars_to_read));
                for (; chars_to_read >= 0; --chars_to_read) {
                    if (m_current == ranges::end(m_range)) {
                        return {error::end_of_range, "EOF"};
                    }
                    ++m_current;
                    auto ret = wrap_in_expected(*m_current);
                    if (!ret) {
                        return ret.error();
                    }
                    m_buffer.push_back(ret.value());
                }
                return {};
            }

            expected<char_type> do_get_at(std::ptrdiff_t i) override
            {
                if (i >= static_cast<std::ptrdiff_t>(m_buffer.size())) {
                    auto e = _read_until_index(i);
                    if (!e) {
                        return e;
                    }
                }
                if (m_read < i + 1 &&
                    static_cast<std::ptrdiff_t>(m_buffer.size()) >= i + 1) {
                    m_read = i + 1;
                }
                return m_buffer[static_cast<size_t>(i)];
            }

            span<const char_type> do_avail_starting_at(
                std::ptrdiff_t i) const override
            {
                if (i >= static_cast<std::ptrdiff_t>(m_buffer.size())) {
                    return {};
                }
                return {m_buffer.data() + i, m_buffer.data() + m_buffer.size()};
            }

            std::ptrdiff_t do_current_index() const override
            {
                return m_read;
            }
            bool do_is_current_at_end() const override
            {
                return m_current == ranges::end(m_range) &&
                       m_read == static_cast<std::ptrdiff_t>(m_buffer.size());
            }

            error do_advance_current(std::ptrdiff_t n) override
            {
                ranges::advance(m_current, n);
                return {};
            }

        protected:
            range_type m_range;
            std::basic_string<char_type> m_buffer{};
            iterator m_current;
            std::ptrdiff_t m_read{0};
        };

        template <typename T>
        using _erased_range_marker = typename T::erased_range_marker;

        template <typename T>
        struct _has_erased_range_marker
            : custom_ranges::detail::exists<_erased_range_marker, T> {
        };
    }  // namespace detail

    template <typename CharT>
    class basic_erased_range {
    public:
        using char_type = CharT;
        using value_type = expected<CharT>;

        class iterator {
            friend class basic_erased_range<CharT>;

        public:
            using value_type = expected<CharT>;
            using reference = expected<CharT>&;
            using pointer = expected<CharT>*;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::bidirectional_iterator_tag;

            iterator() = default;

            expected<CharT> operator*() const
            {
                SCN_EXPECT(m_range);
                auto ret = m_range->m_impl->get_at(m_index);
                return ret;
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

        private:
            iterator(const basic_erased_range<CharT>& r, std::ptrdiff_t i)
                : m_range(const_cast<basic_erased_range<CharT>*>(&r)),
                  m_index(i)
            {
            }

            bool _is_sentinel() const
            {
                if (!m_range) {
                    return true;
                }
                return m_range->m_impl->is_current_at_end() &&
                       m_index == m_range->m_impl->current_index();
            }

            mutable basic_erased_range<CharT>* m_range{nullptr};
            mutable std::ptrdiff_t m_index{0};
        };
        friend class iterator;
        using sentinel = iterator;

        using erased_range_marker = void;
        using skip_erasure_tag = void;

        basic_erased_range() = default;

        template <typename R,
                  typename std::enable_if<!detail::_has_erased_range_marker<
                      detail::remove_cvref_t<R>>::value>::type* = nullptr>
        basic_erased_range(R&& r)
            : m_impl(detail::make_unique<
                     detail::basic_erased_range_impl<R, CharT>>(SCN_FWD(r)))
        {
        }

        iterator begin() const
        {
            return iterator{*this, 0};
        }
        sentinel end() const
        {
            return {};
        }

        span<const char_type> get_buffer(iterator b, std::size_t max_size) const
        {
            SCN_EXPECT(m_impl);
            auto s = m_impl->avail_starting_at(b.m_index);
            if (s.size() > max_size) {
                return s.first(max_size);
            }
            return s;
        }

    private:
        detail::unique_ptr<detail::basic_erased_range_impl_base<char_type>>
            m_impl{nullptr};
    };

    using erased_range = basic_erased_range<char>;
    using werased_range = basic_erased_range<wchar_t>;

    template <typename Range, typename Iterator = ranges::iterator_t<Range>>
    basic_erased_range<typename detail::extract_char_type<Iterator>::type>
    erase_range(Range&& r)
    {
        return {SCN_FWD(r)};
    }

    SCN_END_NAMESPACE
}  // namespace scn

#endif
