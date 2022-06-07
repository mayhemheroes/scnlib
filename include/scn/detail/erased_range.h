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
        template <typename Range>
        struct erased_range_storage_by_value {
            Range m_range;

            erased_range_storage_by_value(Range&& r) : m_range(SCN_MOVE(r)) {}

            Range& get() &
            {
                return m_range;
            }
            const Range& get() const&
            {
                return m_range;
            }
            Range&& get() &&
            {
                return SCN_MOVE(m_range);
            }
        };

        template <typename Range>
        struct erased_range_storage_by_reference {
            Range* m_range;

            erased_range_storage_by_reference(Range& r)
                : m_range(std::addressof(r))
            {
            }

            Range& get()
            {
                return *m_range;
            }
            const Range& get() const
            {
                return *m_range;
            }
        };

        template <typename T>
        using _erased_range_impl_marker = typename T::erased_range_impl_marker;

        template <typename T>
        struct _has_erased_range_impl_marker
            : custom_ranges::detail::exists<_erased_range_impl_marker, T> {
        };

        template <typename StorageType, typename Range, typename CharT>
        class basic_erased_range_impl
            : public basic_erased_range_impl_base<CharT> {
            using base = basic_erased_range_impl_base<CharT>;

        public:
            using char_type = CharT;
            using range_type = Range;
            using range_nocvref_type = remove_cvref_t<range_type>;
            using range_storage_type = StorageType;
            using iterator = ranges::iterator_t<range_type>;
            using sentinel = ranges::sentinel_t<range_type>;

            using erased_range_impl_marker = void;

            template <typename R = range_type,
                      typename std::enable_if<!_has_erased_range_impl_marker<
                          remove_cvref_t<R>>::value>::type* = nullptr>
            basic_erased_range_impl(R&& r)
                : m_range{SCN_FWD(r)}, m_next_to_read_from_source{ranges::begin(m_range.get())}
            {
            }

            basic_erased_range_impl(const basic_erased_range_impl&) = delete;
            basic_erased_range_impl& operator=(const basic_erased_range_impl&) =
                delete;

            basic_erased_range_impl(basic_erased_range_impl&& o)
                : m_range{SCN_MOVE(o.m_range)},
                  m_buffer{SCN_MOVE(o.m_buffer)},
                  m_next_to_read_from_source{ranges::begin(m_range.get())},
                  m_next_char_buffer_index{o.m_next_char_buffer_index}
            {
                auto e = base::advance_current(o.m_next_char_buffer_index);
                SCN_ENSURE(e);
            }
            basic_erased_range_impl& operator=(basic_erased_range_impl&&) =
                delete;

            ~basic_erased_range_impl() override = default;

        private:
            template <typename R>
            std::ptrdiff_t _fill_buffer_impl(std::true_type);
            template <typename R>
            std::ptrdiff_t _fill_buffer_impl(std::false_type);

            [[nodiscard]] std::ptrdiff_t _fill_buffer();

            // read until char index i
            error _read_until_index(std::ptrdiff_t i);

            expected<char_type> do_get_at(std::ptrdiff_t i) override;

            span<const char_type> do_avail_starting_at(
                std::ptrdiff_t i) const override;

            std::ptrdiff_t do_current_index() const override;
            bool do_is_current_at_end() const override;

            error do_advance_current(std::ptrdiff_t n) override;

        protected:
            range_storage_type m_range;
            std::basic_string<char_type> m_buffer{};
            iterator m_next_to_read_from_source;
            std::ptrdiff_t m_next_char_buffer_index{0};
        };

        template <typename T>
        using _erased_range_marker = typename T::erased_range_marker;

        template <typename T>
        struct _has_erased_range_marker
            : custom_ranges::detail::exists<_erased_range_marker, T> {
        };

        template <typename CharT, typename Range>
        auto make_erased_range_impl(Range& r)
            -> basic_erased_range_impl<erased_range_storage_by_reference<Range>,
                                       Range,
                                       CharT>
        {
            return {r};
        }
        template <typename CharT,
                  typename Range,
                  typename std::enable_if<
                      !std::is_reference<Range>::value>::type* = nullptr>
        auto make_erased_range_impl(Range&& r)
            -> basic_erased_range_impl<erased_range_storage_by_value<Range>,
                                       Range,
                                       CharT>
        {
            return {SCN_MOVE(r)};
        }

        template <typename CharT,
                  typename Range,
                  typename ImplType = decltype(make_erased_range_impl<CharT>(
                      SCN_FWD(SCN_DECLVAL(Range))))>
        auto make_unique_erased_range_impl(Range&& r) -> unique_ptr<ImplType>
        {
            return scn::detail::make_unique<ImplType>(
                make_erased_range_impl<CharT>(SCN_FWD(r)));
        }
    }  // namespace detail

    template <typename CharT>
    class basic_erased_range {
    public:
        using char_type = CharT;
        using value_type = expected<CharT>;

        class iterator;

        friend class iterator;
        using sentinel = iterator;

        using erased_range_marker = void;

        basic_erased_range() = default;

        template <typename R,
                  typename std::enable_if<!detail::_has_erased_range_marker<
                      detail::remove_cvref_t<R>>::value>::type* = nullptr>
        basic_erased_range(R&& r)
            : m_impl(detail::make_unique_erased_range_impl<CharT>(SCN_FWD(r)))
        {
        }

        basic_erased_range(iterator, sentinel);

        iterator begin() const noexcept;
        sentinel end() const noexcept;

        span<const char_type> get_buffer(iterator b,
                                         std::size_t max_size) const;

    private:
        detail::unique_ptr<detail::basic_erased_range_impl_base<char_type>>
            m_impl{nullptr};
        std::ptrdiff_t m_begin_index{0};
    };

    using erased_range = basic_erased_range<char>;
    using werased_range = basic_erased_range<wchar_t>;

    template <typename Range, typename Iterator = ranges::iterator_t<Range>>
    basic_erased_range<typename detail::extract_char_type<Iterator>::type>
    erase_range(Range&& r)
    {
        return {SCN_FWD(r)};
    }

    template <typename CharT>
    basic_erased_range<CharT>& erase_range(basic_erased_range<CharT>& r)
    {
        return r;
    }
    template <typename CharT>
    basic_erased_range<CharT> erase_range(basic_erased_range<CharT>&& r)
    {
        return SCN_MOVE(r);
    }

    template <typename CharT>
    class basic_erased_view {
    public:
        using range_type = basic_erased_range<CharT>;
        using char_type = CharT;
        using value_type = expected<CharT>;
        using iterator = typename range_type::iterator;
        using sentinel = typename range_type::sentinel;

        basic_erased_view() noexcept = default;
        basic_erased_view(range_type& range) noexcept
            : m_begin(range.begin()), m_end(range.end())
        {
        }
        basic_erased_view(iterator begin, sentinel end) noexcept
            : m_begin(SCN_MOVE(begin)), m_end(SCN_MOVE(end))
        {
        }

        iterator begin() const noexcept
        {
            return m_begin;
        }

        sentinel end() const noexcept
        {
            return m_end;
        }

        bool empty() const noexcept
        {
            return begin() == end();
        }

        span<const char_type> get_buffer(iterator b, std::size_t max_size) const
        {
            SCN_EXPECT(m_begin.get_range());
            return get().get_buffer(b, max_size);
        }

        range_type& get() &
        {
            SCN_EXPECT(m_begin.get_range() != nullptr);
            return *m_begin.get_range();
        }
        const range_type& get() const&
        {
            SCN_EXPECT(m_begin.get_range() != nullptr);
            return *m_begin.get_range();
        }
        range_type get() &&
        {
            SCN_EXPECT(m_begin.get_range() != nullptr);
            return SCN_MOVE(*m_begin.get_range());
        }

    private:
        iterator m_begin{};
        sentinel m_end{};
    };

    using erased_view = basic_erased_view<char>;
    using werased_view = basic_erased_view<wchar_t>;

    SCN_END_NAMESPACE
}  // namespace scn

#include "erased_range_impl.h"

#endif
