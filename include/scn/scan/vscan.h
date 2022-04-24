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

#ifndef SCN_SCAN_VSCAN_H
#define SCN_SCAN_VSCAN_H

#include "../detail/context.h"
#include "../detail/erased_range.h"
#include "../detail/file.h"
#include "../detail/parse_context.h"
#include "../detail/visitor.h"
#include "common.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    // Avoid documentation issues: without this, Doxygen will think
    // SCN_BEGIN_NAMESPACE is a part of the vscan declaration
    namespace dummy {
    }

    /**
     * Type returned by `vscan` and others
     */
    template <typename Range>
    struct vscan_result {
        error err;
        Range range;
    };

    namespace detail {
        template <typename WrappedRange,
                  typename CharT = typename WrappedRange::char_type>
        vscan_result<typename WrappedRange::range_type> vscan_boilerplate(
            WrappedRange&& r,
            basic_string_view<CharT> fmt,
            basic_args<CharT> args)
        {
            auto ctx = make_context(SCN_MOVE(r));
            auto pctx = make_parse_context(fmt, ctx.locale());
            auto err = visit(ctx, pctx, SCN_MOVE(args));
            return {err, SCN_MOVE(ctx.range())};
        }

        template <typename WrappedRange,
                  typename CharT = typename WrappedRange::char_type>
        vscan_result<typename WrappedRange::range_type>
        vscan_boilerplate_default(WrappedRange&& r,
                                  int n_args,
                                  basic_args<CharT> args)
        {
            auto ctx = make_context(SCN_MOVE(r));
            auto pctx = make_parse_context(n_args, ctx.locale());
            auto err = visit(ctx, pctx, SCN_MOVE(args));
            return {err, SCN_MOVE(ctx.range())};
        }

        template <typename WrappedRange,
                  typename Format,
                  typename CharT = typename WrappedRange::char_type>
        vscan_result<typename WrappedRange::range_type>
        vscan_boilerplate_localized(WrappedRange&& r,
                                    basic_locale_ref<CharT>&& loc,
                                    const Format& fmt,
                                    basic_args<CharT> args)
        {
            auto ctx = make_context(SCN_MOVE(r), SCN_MOVE(loc));
            auto pctx = make_parse_context(fmt, ctx.locale());
            auto err = visit(ctx, pctx, SCN_MOVE(args));
            return {err, SCN_MOVE(ctx.range())};
        }
    }  // namespace detail

#define SCN_VSCAN_DECLARE(Range, WrappedRange, CharT)                     \
    vscan_result<Range> vscan(Range, basic_string_view<CharT>,            \
                              basic_args<CharT>&&);                       \
                                                                          \
    vscan_result<Range> vscan_default(Range, int, basic_args<CharT>&&);   \
                                                                          \
    vscan_result<Range> vscan_localized(Range, basic_locale_ref<CharT>&&, \
                                        basic_string_view<CharT>,         \
                                        basic_args<CharT>&&);             \
                                                                          \
    error vscan_usertype(basic_context<WrappedRange>&,                    \
                         basic_string_view<CharT>, basic_args<CharT>&&)

    SCN_VSCAN_DECLARE(string_view, string_view_wrapper, char);
    SCN_VSCAN_DECLARE(wstring_view, wstring_view_wrapper, wchar_t);
    SCN_VSCAN_DECLARE(erased_view, erased_view_wrapper, char);
    SCN_VSCAN_DECLARE(werased_view, werased_view_wrapper, wchar_t);

    SCN_END_NAMESPACE
}  // namespace scn

#if defined(SCN_HEADER_ONLY) && SCN_HEADER_ONLY && !defined(SCN_VSCAN_CPP)
#include "vscan.cpp"
#endif

#endif  // SCN_SCAN_VSCAN_H
