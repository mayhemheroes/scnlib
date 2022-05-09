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

#if defined(SCN_HEADER_ONLY) && SCN_HEADER_ONLY
#define SCN_VSCAN_CPP
#endif

#include <scn/scan/vscan.h>

#include <scn/detail/context.h>
#include <scn/detail/parse_context.h>
#include <scn/detail/visitor.h>

namespace scn {
    SCN_BEGIN_NAMESPACE

#define SCN_VSCAN_DEFINE(Range, WrappedRange, CharT)                          \
    vscan_result<Range> vscan(Range r, basic_string_view<CharT> f,            \
                              basic_args<CharT>&& a)                          \
    {                                                                         \
        return detail::vscan_boilerplate(wrap(r), f, SCN_MOVE(a));            \
    }                                                                         \
                                                                              \
    vscan_result<Range> vscan_default(Range r, int n, basic_args<CharT>&& a)  \
    {                                                                         \
        return detail::vscan_boilerplate_default(wrap(r), n, SCN_MOVE(a));    \
    }                                                                         \
                                                                              \
    vscan_result<Range> vscan_localized(                                      \
        Range r, basic_locale_ref<CharT>&& loc, basic_string_view<CharT> f,   \
        basic_args<CharT>&& a)                                                \
    {                                                                         \
        return detail::vscan_boilerplate_localized(wrap(r), SCN_MOVE(loc), f, \
                                                   SCN_MOVE(a));              \
    }                                                                         \
                                                                              \
    error vscan_usertype(basic_context<WrappedRange>& ctx,                    \
                         basic_string_view<CharT> f, basic_args<CharT>&& a)   \
    {                                                                         \
        auto pctx = make_parse_context(f, ctx.locale());                      \
        return visit(ctx, pctx, SCN_MOVE(a));                                 \
    }

    SCN_VSCAN_DEFINE(string_view, string_view_wrapper, char)
    SCN_VSCAN_DEFINE(wstring_view, wstring_view_wrapper, wchar_t)
    SCN_VSCAN_DEFINE(erased_view, erased_view_wrapper, char)
    SCN_VSCAN_DEFINE(werased_view, werased_view_wrapper, wchar_t)

    SCN_END_NAMESPACE
}  // namespace scn
