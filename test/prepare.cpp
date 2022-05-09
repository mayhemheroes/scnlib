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

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "test.h"

TEST_CASE("prepare")
{
    SUBCASE("string literal -> string_view")
    {
        auto r = scn::prepare("123 456");

        static_assert(
            std::is_same<decltype(r),
                         scn::ready_prepared_range<scn::string_view>>::value,
            "");
        static_assert(std::is_same<decltype(r.get()), scn::string_view&>::value,
                      "");
    }
    SUBCASE("string_view -> string_view")
    {
        auto r = scn::prepare(scn::string_view{"123 456"});

        static_assert(
            std::is_same<decltype(r),
                         scn::ready_prepared_range<scn::string_view>>::value,
            "");
        static_assert(std::is_same<decltype(r.get()), scn::string_view&>::value,
                      "");
    }
    SUBCASE("span -> string_view")
    {
        auto str = scn::string_view{"123 456"};
        auto r = scn::prepare(scn::span<const char>{str.data(), str.size()});

        static_assert(
            std::is_same<decltype(r),
                         scn::ready_prepared_range<scn::string_view>>::value,
            "");
        static_assert(std::is_same<decltype(r.get()), scn::string_view&>::value,
                      "");
    }
    SUBCASE("lvalue string -> string_view")
    {
        auto str = std::string{"123 456"};
        auto r = scn::prepare(str);

        static_assert(
            std::is_same<decltype(r),
                         scn::ready_prepared_range<scn::string_view>>::value,
            "");
        static_assert(std::is_same<decltype(r.get()), scn::string_view&>::value,
                      "");
    }
    SUBCASE("rvalue string -> string")
    {
        auto r = scn::prepare(std::string{"123 456"});

        static_assert(
            std::is_same<decltype(r),
                         scn::pending_prepared_range<std::string,
                                                     scn::string_view>>::value,
            "");
        static_assert(std::is_same<decltype(r.get()), scn::string_view>::value,
                      "");
    }
    SUBCASE("lvalue file -> lvalue file")
    {
        auto f = scn::file{};
        auto r = scn::prepare(f);

        static_assert(
            std::is_same<decltype(r),
                         scn::pending_prepared_range<scn::erased_range,
                                                     scn::erased_view>>::value,
            "");
        static_assert(std::is_same<decltype(r.get()), scn::erased_view>::value,
                      "");
    }
    SUBCASE("rvalue file -> rvalue file")
    {
        auto r = scn::prepare(scn::file{});

        static_assert(
            std::is_same<decltype(r),
                         scn::pending_prepared_range<scn::erased_range,
                                                     scn::erased_view>>::value,
            "");
        static_assert(std::is_same<decltype(r.get()), scn::erased_view>::value,
                      "");
    }
    SUBCASE("lvalue other -> erased")
    {
        auto s = get_deque<char>("123");
        auto r = scn::prepare(s);

        static_assert(
            std::is_same<decltype(r),
                         scn::pending_prepared_range<scn::erased_range,
                                                     scn::erased_view>>::value,
            "");
        static_assert(std::is_same<decltype(r.get()), scn::erased_view>::value,
                      "");
    }
    SUBCASE("rvalue other -> erased")
    {
        auto r = scn::prepare(get_deque<char>("123"));

        static_assert(
            std::is_same<decltype(r),
                         scn::pending_prepared_range<scn::erased_range,
                                                     scn::erased_view>>::value,
            "");
        static_assert(std::is_same<decltype(r.get()), scn::erased_view>::value,
                      "");
    }
}
