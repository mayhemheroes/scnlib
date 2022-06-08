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

TEST_CASE_TEMPLATE("boolean", CharT, char, wchar_t)
{
    auto default_format = widen<CharT>("{}");
    auto s_format = widen<CharT>("{:s}");
    auto i_format = widen<CharT>("{:i}");
    auto L_format = widen<CharT>("{:L}");
    auto n_format = widen<CharT>("{:n}");

    {
        auto source = widen<CharT>("true");
        bool b{};
        auto e = scn::scan(source, default_format, b);
        CHECK(b);
        CHECK(e);
    }
    {
        auto source = widen<CharT>("false");
        bool b{};
        auto e = scn::scan(source, default_format, b);
        CHECK(!b);
        CHECK(e);
    }
    {
        auto source = widen<CharT>("true");
        bool b{};
        auto e = scn::scan(source, s_format, b);
        CHECK(b);
        CHECK(e);
    }
    {
        auto source = widen<CharT>("false");
        bool b{};
        auto e = scn::scan(source, s_format, b);
        CHECK(!b);
        CHECK(e);
    }
    {
        auto source = widen<CharT>("bool");
        bool b{};
        auto e = scn::scan(source, s_format, b);
        CHECK(!e);
        CHECK(e.error() == scn::error::invalid_scanned_value);
    }
    {
        auto source = widen<CharT>("0");
        bool b{};
        auto e = scn::scan(source, s_format, b);
        CHECK(!e);
        CHECK(e.error() == scn::error::invalid_scanned_value);
    }
    {
        auto source = widen<CharT>("0");
        bool b{};
        auto e = scn::scan(source, default_format, b);
        CHECK(!b);
        CHECK(e);
    }
    {
        auto source = widen<CharT>("1");
        bool b{};
        auto e = scn::scan(source, default_format, b);
        CHECK(b);
        CHECK(e);
    }
    {
        auto source = widen<CharT>("0");
        bool b{};
        auto e = scn::scan(source, i_format, b);
        CHECK(!b);
        CHECK(e);
    }
    {
        auto source = widen<CharT>("1");
        bool b{};
        auto e = scn::scan(source, i_format, b);
        CHECK(b);
        CHECK(e);
    }
    {
        auto source = widen<CharT>("0");
        bool b{};
        auto e = scn::scan(source, L_format, b);
        CHECK(!b);
        CHECK(e);
    }
    {
        auto source = widen<CharT>("1");
        bool b{};
        auto e = scn::scan(source, L_format, b);
        CHECK(b);
        CHECK(e);
    }
    {
        auto source = widen<CharT>("2");
        bool b{};
        auto e = scn::scan(source, default_format, b);
        CHECK(!e);
        CHECK(e.error() == scn::error::invalid_scanned_value);
    }
    {
        auto source = widen<CharT>("true");
        bool b{};
        auto e = scn::scan(source, n_format, b);
        CHECK(b);
        CHECK(e);
    }
    {
        auto source = widen<CharT>("0");
        bool b{};
        auto e = scn::scan(source, n_format, b);
        CHECK(!b);
        CHECK(e);
    }
}
