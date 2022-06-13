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

#ifndef SCN_DETAIL_RANGE_H
#define SCN_DETAIL_RANGE_H

#include "../ranges/ranges.h"
#include "../util/algorithm.h"
#include "../util/memory.h"
#include "erased_range.h"
#include "error.h"
#include "prepare.h"
#include "vectored.h"
#include "wrapper.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    namespace detail {
        template <typename Range>
        struct reconstruct_tag {
        };

        template <
            typename Range,
            typename Iterator,
            typename Sentinel,
            typename = typename std::enable_if<
                std::is_constructible<Range, Iterator, Sentinel>::value>::type>
        Range reconstruct(reconstruct_tag<Range>, Iterator begin, Sentinel end)
        {
            return {begin, end};
        }
#if SCN_HAS_STRING_VIEW
        // std::string_view is not reconstructible pre-C++20
        template <typename CharT,
                  typename Traits,
                  typename Iterator,
                  typename Sentinel>
        std::basic_string_view<CharT, Traits> reconstruct(
            reconstruct_tag<std::basic_string_view<CharT, Traits>>,
            Iterator begin,
            Sentinel end)
        {
            // On MSVC, string_view can't even be constructed from its
            // iterators!
            return {::scn::detail::to_address(begin),
                    static_cast<size_t>(ranges::distance(begin, end))};
        }
#endif  // SCN_HAS_STRING_VIEW
    }   // namespace detail

    SCN_END_NAMESPACE
}  // namespace scn

#endif  // SCN_DETAIL_RANGE_H
