// Copyright 2017-2019 Elias Kosunen
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

#ifndef SCN_SCN_H
#define SCN_SCN_H

#include "detail/args.h"
#include "detail/context.h"
#include "detail/core.h"
#include "detail/locale.h"
#include "detail/options.h"
#include "detail/result.h"
#include "detail/stream.h"
#include "detail/types.h"
#include "detail/visitor.h"
#include "detail/vscan.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    template <typename Stream, typename... Args>
    result<int> scan(Stream& s,
                     basic_string_view<typename Stream::char_type> f,
                     Args&... a)
    {
        static_assert(sizeof...(Args) > 0,
                      "Have to scan at least a single argument");
        SCN_EXPECT(!f.empty());

        using context_type = typename erased_stream_context_type<Stream>::type;

        auto args = make_args<context_type>(a...);
        auto ctx = context_type(s, f, args);
        return vscan(ctx);
    }
    template <typename Stream, typename... Args>
    result<int> scan(options opt,
                     Stream& s,
                     basic_string_view<typename Stream::char_type> f,
                     Args&... a)
    {
        static_assert(sizeof...(Args) > 0,
                      "Have to scan at least a single argument");
        SCN_EXPECT(!f.empty());

        using context_type = typename erased_stream_context_type<Stream>::type;

        auto args = make_args<context_type>(a...);
        auto ctx = context_type(s, f, args, opt);
        return vscan(ctx);
    }

    SCN_CLANG_PUSH
    SCN_CLANG_IGNORE("-Wexit-time-destructors")

    // Reference to global stdin stream.
    // Not safe to use during static construction or destruction.
    template <typename CharT>
    erased_stream<CharT>& stdin_stream()
    {
        static basic_cstdio_stream<CharT> s(stdin);
        static erased_stream<CharT> stream(s);
        return stream;
    }
    inline erased_stream<char>& cstdin()
    {
        return stdin_stream<char>();
    }
    inline erased_stream<wchar_t>& wcstdin()
    {
        return stdin_stream<wchar_t>();
    }

    SCN_CLANG_POP

    // Read from stdin
    template <typename... Args>
    result<int> input(string_view f, Args&... a)
    {
        static_assert(sizeof...(Args) > 0,
                      "Have to scan at least a single argument");
        SCN_EXPECT(!f.empty());

        auto& stream = stdin_stream<char>();

        using stream_type =
            typename std::remove_reference<decltype(stream)>::type;
        using context_type =
            typename erased_stream_context_type<stream_type>::type;

        auto args = make_args<context_type>(a...);
        auto ctx = context_type(stream, f, args);
        return vscan(ctx);
    }
    template <typename... Args>
    result<int> input(wstring_view f, Args&... a)
    {
        static_assert(sizeof...(Args) > 0,
                      "Have to scan at least a single argument");
        SCN_EXPECT(!f.empty());

        auto& stream = stdin_stream<wchar_t>();

        using stream_type =
            typename std::remove_reference<decltype(stream)>::type;
        using context_type =
            typename erased_stream_context_type<stream_type>::type;

        auto args = make_args<context_type>(a...);
        auto ctx = context_type(stream, f, args);
        return vscan(ctx);
    }

    // Read from stdin with prompt
    // Like Python's input()
    template <typename... Args>
    result<int> prompt(const char* p, string_view f, Args&... a)
    {
        static_assert(sizeof...(Args) > 0,
                      "Have to scan at least a single argument");
        SCN_EXPECT(!f.empty());
        SCN_EXPECT(p != nullptr);

        std::printf("%s", p);

        auto& stream = stdin_stream<char>();
        using stream_type =
            typename std::remove_reference<decltype(stream)>::type;
        using context_type =
            typename erased_stream_context_type<stream_type>::type;

        auto args = make_args<context_type>(a...);
        auto ctx = context_type(stream, f, args);
        return vscan(ctx);
    }
    template <typename... Args>
    result<int> prompt(const wchar_t* p, wstring_view f, Args&... a)
    {
        static_assert(sizeof...(Args) > 0,
                      "Have to scan at least a single argument");
        SCN_EXPECT(!f.empty());
        SCN_EXPECT(p != nullptr);

        std::wprintf(L"%ls", p);

        auto& stream = stdin_stream<wchar_t>();
        using stream_type =
            typename std::remove_reference<decltype(stream)>::type;
        using context_type =
            typename erased_stream_context_type<stream_type>::type;

        auto args = make_args<context_type>(a...);
        auto ctx = context_type(stream, f, args);
        return vscan(ctx);
    }

    SCN_END_NAMESPACE
}  // namespace scn

#if defined(SCN_HEADER_ONLY) && SCN_HEADER_ONLY && \
    !defined(SCN_DETAIL_VSCAN_CPP)
#include "vscan.cpp"
#endif

#endif  // SCN_SCN_H

