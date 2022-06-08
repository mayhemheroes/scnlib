// Copyright 2017 Elias Kosunen
//
// Licensed under the Apache License, Version 2.0 (the "License{");
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

static_assert(SCN_CHECK_CONCEPT(scn::ranges::view<scn::string_view>), "");
static_assert(!SCN_CHECK_CONCEPT(scn::ranges::view<const char*>), "");
static_assert(!SCN_CHECK_CONCEPT(scn::ranges::view<const char (&)[2]>), "");

TEST_CASE("string_view range_wrapper")
{
    auto source = scn::string_view{"123 456"};
    auto prepared = scn::prepare(source);
    auto wrapped = scn::wrap(prepared);
    auto range = scn::wrap(wrapped);

    static_assert(
        std::is_same<decltype(range), scn::string_view_wrapper>::value, "");
}

TEST_CASE("erased_range")
{
    auto source = scn::erase_range("123");
    auto prepared = scn::prepare(source);
    auto range = scn::wrap(prepared);

    static_assert(
        std::is_same<decltype(range), scn::erased_view_wrapper>::value, "");
}

// mapped_file has .wrap() member function
TEST_CASE("mapped_file")
{
    auto file = scn::mapped_file{};
    auto prepared = scn::prepare(file);
    auto range = scn::wrap(prepared);

    static_assert(
        std::is_same<decltype(range), scn::string_view_wrapper>::value, "");
}

TEST_CASE("string_view")
{
    auto source = scn::string_view{"123"};
    auto prepared = scn::prepare(source);
    auto range = scn::wrap(prepared);

    static_assert(
        std::is_same<decltype(range), scn::string_view_wrapper>::value, "");
}

TEST_CASE("span")
{
    auto source = scn::span<char>{};
    auto prepared = scn::prepare(source);
    auto range = scn::wrap(prepared);

    static_assert(
        std::is_same<decltype(range), scn::string_view_wrapper>::value, "");
}

TEST_CASE("string")
{
    auto source = std::string{};
    auto prepared = scn::prepare(source);
    auto range = scn::wrap(prepared);

    static_assert(
        std::is_same<decltype(range), scn::string_view_wrapper>::value, "");
}

TEST_CASE("file")
{
    auto source = scn::file{};
    auto erased = scn::erase_range(source);
    auto range = scn::wrap(scn::prepare(erased));

    static_assert(
        std::is_same<decltype(range), scn::erased_view_wrapper>::value, "");
}
