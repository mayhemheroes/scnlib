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
#include <scn/istream.h>
#include <istream>
#include "../test.h"

static bool do_fgets(char* str, size_t count, std::FILE* f)
{
    return std::fgets(str, static_cast<int>(count), f) != nullptr;
}
static bool do_fgets(wchar_t* str, size_t count, std::FILE* f)
{
    return std::fgetws(str, static_cast<int>(count), f) != nullptr;
}

static void set_cfile_width(FILE* file, char)
{
    std::fwide(file, -1);
}
static void set_cfile_width(FILE* file, wchar_t)
{
    std::fwide(file, 1);
}

template <typename CharT>
struct cfile_wrapper {
    cfile_wrapper(const char* file, const char* flags)
        : f{std::fopen(file, flags)}
    {
        set_cfile_width(f, CharT{});
    }
    ~cfile_wrapper()
    {
        std::fclose(f);
    }

    FILE* f;
};

TEST_CASE("file range single characters")
{
    cfile_wrapper<char> cfile{"./test/file/testfile.txt", "r"};
    scn::file file{cfile.f};

    auto it = file.begin();
    auto ch = *it;
    CHECK(ch);
    CHECK(ch.value() == '1');
    ch = *it;
    CHECK(ch.value() == '1');

    ++it;
    ch = *it;
    CHECK(ch);
    CHECK(ch.value() == '2');
}

TEST_CASE_TEMPLATE("file range", CharT, char, wchar_t)
{
    cfile_wrapper<CharT> cfile{"./test/file/testfile.txt", "r"};
    scn::basic_file<CharT> file{cfile.f};

    std::basic_string<CharT> dest;
    auto out = std::back_inserter(dest);

    for (auto it = file.begin(); it != file.end(); ++it) {
        auto ch = *it;
        CHECK(ch);
        *out++ = ch.value();
    }
    CHECK(dest == widen<CharT>("123\nword another"));
}

TEST_CASE_TEMPLATE("file", CharT, char, wchar_t)
{
    cfile_wrapper<CharT> cfile{"./test/file/testfile.txt", "r"};
    scn::basic_file<CharT> file{cfile.f};

    using string_type = std::basic_string<CharT>;

    SUBCASE("basic read")
    {
        std::basic_string<CharT> buffer;
        auto out = std::back_inserter(buffer);

        auto wrapped = scn::wrap(scn::prepare(file));
        auto locale = scn::basic_locale_ref<CharT>{};
        auto is_space = scn::detail::make_is_space_predicate(locale, false);
        auto ret = scn::read_until_space(wrapped, out, is_space, false);

        CHECK(ret);
        CHECK(buffer == widen<CharT>("123"));
    }
    SUBCASE("basic scan")
    {
        int i;
        auto result = scn::scan_default(file, i);
        CHECK(result);
        CHECK(i == 123);
    }
    SUBCASE("entire file")
    {
        auto result = scn::make_result(file);

        int i;
        result = scn::scan_default(result.range(), i);
        CHECK(result);
        CHECK(i == 123);

        string_type word;
        result = scn::scan_default(result.range(), word);
        CHECK(result);
        CHECK(word == widen<CharT>("word"));

        result = scn::scan_default(result.range(), word);
        CHECK(result);
        CHECK(word == widen<CharT>("another"));

        result = scn::scan_default(result.range(), word);
        CHECK(!result);
        CHECK(result.error().code() == scn::error::end_of_range);
        CHECK(word == widen<CharT>("another"));
    }

    SUBCASE("syncing")
    {
        int i;
        auto result = scn::scan_default(file, i);
        CHECK(result);
        CHECK(i == 123);
        file.sync();

        string_type word;
        result = scn::scan_default(file, word);
        CHECK(result);
        CHECK(word == widen<CharT>("word"));
        file.sync();

        word = widen<CharT>("another");

        std::vector<CharT> buf(word.size() + 1, 0);
        bool fgets_ret = do_fgets(buf.data(), buf.size(), file.handle());
        CHECK(fgets_ret);
        CHECK(word == string_type{buf.data()});
        CHECK(std::ferror(file.handle()) == 0);
        CHECK(std::feof(file.handle()) == 0);
    }

    SUBCASE("not syncing")
    {
        int i;
        auto result = scn::scan_default(file, i);
        CHECK(result);
        CHECK(i == 123);

        // syncing required to use original `file`
        string_type word;
        result = scn::scan_default(file, word);
        CHECK(result);
        CHECK(word == widen<CharT>("123"));

        // syncing required to use the file handle
        word = widen<CharT>("word");
        std::vector<CharT> buf(word.size() + 1, 0);
        bool fgets_ret = do_fgets(buf.data(), buf.size(), file.handle());
        CHECK(fgets_ret);
        CHECK(word == string_type{buf.data()});
        CHECK(std::ferror(file.handle()) == 0);
        CHECK(std::feof(file.handle()) == 0);
    }

    SUBCASE("error")
    {
        int i;
        auto result = scn::scan_default(file, i);
        CHECK(result);
        CHECK(i == 123);

        result = scn::scan_default(result.range(), i);
        CHECK(!result);
        CHECK(result.error().code() == scn::error::invalid_scanned_value);
        CHECK(i == 123);

        // can still read again
        string_type word;
        result = scn::scan_default(result.range(), word);
        CHECK(result);
        CHECK(word == widen<CharT>("word"));
    }

    SUBCASE("getline")
    {
        string_type line;
        auto result = scn::getline(file, line);
        CHECK(result);
        CHECK(line == widen<CharT>("123"));

        result = scn::getline(result.range(), line);
        CHECK(result);
        CHECK(line == widen<CharT>("word another"));

        result = scn::getline(result.range(), line);
        CHECK(!result);
        CHECK(result.error().code() == scn::error::end_of_range);
        CHECK(line == widen<CharT>("word another"));
    }
}

TEST_CASE("mapped file")
{
    scn::mapped_file file{"./test/file/testfile.txt"};
    REQUIRE(file.valid());

    SUBCASE("basic")
    {
        int i;
        auto result = scn::scan_default(file, i);
        CHECK(result);
        CHECK(i == 123);
    }
    SUBCASE("entire file")
    {
        auto result = scn::make_result(file);

        int i;
        result = scn::scan_default(result.range(), i);
        CHECK(result);
        CHECK(i == 123);

        std::string word;
        result = scn::scan_default(result.range(), word);
        CHECK(result);
        CHECK(word == "word");

        result = scn::scan_default(result.range(), word);
        CHECK(result);
        CHECK(word == "another");

        result = scn::scan_default(result.range(), word);
        CHECK(!result);
        CHECK(result.error().code() == scn::error::end_of_range);
        CHECK(word == "another");
    }
}

struct int_and_string {
    int i;
    std::string s;
};
struct two_strings {
    std::string first, second;
};
struct istream_int_and_string {
    int i;
    std::string s;

    friend std::istream& operator>>(std::istream& is,
                                    istream_int_and_string& val)
    {
        is >> val.i >> val.s;
        return is;
    }
};

namespace scn {
    template <>
    struct scanner<int_and_string> : public scn::empty_parser {
        template <typename Context>
        error scan(int_and_string& val, Context& ctx)
        {
            return scn::scan_usertype(ctx, "{} {}", val.i, val.s);
        }
    };

    template <>
    struct scanner<two_strings> : public scn::empty_parser {
        template <typename Context>
        error scan(two_strings& val, Context& ctx)
        {
            return scn::scan_usertype(ctx, "{} {}", val.first, val.second);
#if 0
            auto r = scn::scan(ctx.range(), "{} {}", val.first, val.second);
            ctx.range() = std::move(r.range());
            return r.error();
#endif
        }
    };
}  // namespace scn

TEST_CASE("file usertype")
{
    cfile_wrapper<char> cfile{"./test/file/testfile.txt", "r"};
    scn::file file{cfile.f};

    SUBCASE("int_and_string")
    {
        int_and_string val{};
        auto result = scn::scan_default(file, val);
        CHECK(result);
        CHECK(val.i == 123);
        CHECK(val.s == "word");

        std::string s;
        result = scn::scan_default(result.range(), s);
        CHECK(result);
        CHECK(s == "another");
    }

    SUBCASE("int_and_string failure")
    {
        int i;
        auto result = scn::scan_default(file, i);
        CHECK(result);
        CHECK(i == 123);

        int_and_string val{};
        result = scn::scan_default(result.range(), val);
        CHECK(!result);
        CHECK(result.error().code() == scn::error::invalid_scanned_value);

        std::string s;
        result = scn::scan_default(result.range(), s);
        CHECK(result);
        CHECK(s == "word");
    }

    SUBCASE("two_strings")
    {
        two_strings val{};
        auto result = scn::scan_default(file, val);
        CHECK(result);
        CHECK(val.first == "123");
        CHECK(val.second == "word");

        std::string s;
        result = scn::scan_default(result.range(), s);
        CHECK(result);
        CHECK(s == "another");
    }

    SUBCASE("istream")
    {
        istream_int_and_string val{};
        auto result = scn::scan_default(file, val);
        CHECK(result);
        CHECK(val.i == 123);
        CHECK(val.s == "word");

        std::string s;
        result = scn::scan_default(result.range(), s);
        CHECK(result);
        CHECK(s == "another");
    }
    SUBCASE("istream failure")
    {
        int i;
        auto result = scn::scan_default(file, i);
        CHECK(result);
        CHECK(i == 123);

        istream_int_and_string val{};
        result = scn::scan_default(result.range(), val);
        CHECK(!result);
        CHECK(result.error().code() == scn::error::invalid_scanned_value);

        std::string s;
        result = scn::scan_default(result.range(), s);
        CHECK(result);
        CHECK(s == "word");
    }
}
