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

#ifndef SCN_DETAIL_PREPARE_H
#define SCN_DETAIL_PREPARE_H

#include "erased_range.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

#if 0
    template <typename Range>
    struct ready_prepared_range {
        using target_type = Range;
        using stored_type = Range;

        Range range;

        Range& get()
        {
            return range;
        }
    };

    template <typename StoredRange, typename TargetRange>
    struct pending_prepared_range {
        using target_type = TargetRange;
        using stored_type = StoredRange;

        StoredRange range;

        TargetRange get();
    };
#endif

    namespace _prepare {
        // erased_range& -> ready<erased_view>
        // erased_view -> ready<erased_view>
        // REMOVED: string&& -> pending<string, string_view>
        // string-like -> ready<string_view>
        // REMOVED: other -> pending<erased_range, erased_view>
        struct fn {
        private:
            template <typename CharT>
            static basic_erased_view<CharT> impl(
                basic_erased_range<CharT>& range,
                detail::priority_tag<
                    4>) noexcept(std::
                                     is_nothrow_constructible<
                                         basic_erased_view<CharT>,
                                         basic_erased_range<CharT>&>::value)
            {
                return {range};
            }
            template <typename CharT>
            static basic_erased_view<CharT> impl(
                basic_erased_view<CharT> range,
                detail::priority_tag<4>) noexcept
            {
                return {SCN_MOVE(range)};
            }

            template <typename CharT, std::size_t N>
            static basic_string_view<typename std::remove_cv<CharT>::type> impl(
                CharT (&s)[N],
                detail::priority_tag<3>) noexcept
            {
                return {s, s + N - 1};
            }

            template <typename CharT>
            static basic_string_view<CharT> impl(
                basic_string_view<CharT> s,
                detail::priority_tag<2>) noexcept
            {
                return {s};
            }

#if SCN_HAS_STRING_VIEW
            template <typename CharT>
            static basic_string_view<CharT> impl(
                std::basic_string_view<CharT> s,
                detail::priority_tag<2>) noexcept
            {
                return {s};
            }
#endif

            template <typename CharT>
            static basic_string_view<typename std::remove_cv<CharT>::type> impl(
                span<CharT> s,
                detail::priority_tag<2>) noexcept
            {
                return {s.data(), s.size()};
            }

            template <typename CharT, typename Allocator>
            static basic_string_view<CharT> impl(
                std::basic_string<CharT, std::char_traits<CharT>, Allocator>&
                    str,
                detail::priority_tag<2>) noexcept
            {
                return {str.data(), str.size()};
            }

#if 0
            template <typename CharT, typename Allocator>
            static pending_prepared_range<
                std::basic_string<CharT, std::char_traits<CharT>, Allocator>,
                basic_string_view<CharT>>
            impl(std::basic_string<CharT, std::char_traits<CharT>, Allocator>&&
                     str,
                 detail::priority_tag<
                     2>) noexcept(std::
                                      is_nothrow_move_constructible<
                                          decltype(str)>::value)
            {
                return {SCN_MOVE(str)};
            }
#endif

            template <typename T,
                      typename = decltype(SCN_DECLVAL(T&).prepare())>
            static auto impl(T& r, detail::priority_tag<1>) noexcept(
                noexcept(r.prepare())) -> decltype(r.prepare())
            {
                return r.prepare();
            }

            template <typename T,
                      typename std::enable_if<
                          !std::is_reference<T>::value>::type* = nullptr,
                      typename = decltype(SCN_DECLVAL(T &&).prepare())>
            static auto impl(T&& r, detail::priority_tag<1>) noexcept(noexcept(
                SCN_MOVE(r).prepare())) -> decltype(SCN_MOVE(r).prepare())
            {
                return SCN_MOVE(r).prepare();
            }

#if 0
            template <typename T>
            static auto impl(T&& r, detail::priority_tag<0>) noexcept(
                noexcept(erase_range(SCN_FWD(r))))
                -> pending_prepared_range<
                    decltype(erase_range(SCN_FWD(r))),
                    basic_erased_view<
                        typename decltype(erase_range(SCN_FWD(r)))::char_type>>
            {
                return {erase_range(SCN_FWD(r))};
            }
#else
            template <typename T>
            static auto impl(T&& r, detail::priority_tag<0>) noexcept -> T
            {
                static_assert(detail::dependent_false<T>::value,
                              "prepare given an invalid range");
            }
#endif

        public:
            template <typename T>
            auto operator()(T&& r) const
                noexcept(noexcept(fn::impl(SCN_FWD(r),
                                           detail::priority_tag<4>{})))
                    -> decltype(fn::impl(SCN_FWD(r), detail::priority_tag<4>{}))
            {
                return fn::impl(SCN_FWD(r), detail::priority_tag<4>{});
            }
        };
    }  // namespace _prepare
    namespace {
        static constexpr auto& prepare =
            detail::static_const<_prepare::fn>::value;
    }

#if 0
    template <typename StoredRange, typename TargetRange>
    inline TargetRange pending_prepared_range<StoredRange, TargetRange>::get()
    {
        return prepare(range).get();
    }
#endif

    SCN_END_NAMESPACE
}  // namespace scn

#endif
