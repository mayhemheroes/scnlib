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
#define SCN_FILE_CPP
#endif

#include <scn/detail/error.h>
#include <scn/detail/file.h>
#include <scn/detail/locale.h>
#include <scn/util/expected.h>

#include <cstdio>

#if SCN_POSIX
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#elif SCN_WINDOWS

#ifdef WIN32_LEAN_AND_MEAN
#define SCN_WIN32_LEAN_DEFINED 1
#else
#define WIN32_LEAN_AND_MEAN
#define SCN_WIN32_LEAN_DEFINED 0
#endif

#ifdef NOMINMAX
#define SCN_NOMINMAX_DEFINED 1
#else
#define NOMINMAX
#define SCN_NOMINMAX_DEFINED 0
#endif

#include <Windows.h>

#if !SCN_NOMINMAX_DEFINED
#undef NOMINMAX
#endif

#if !SCN_WIN32_LEAN_DEFINED
#undef WIN32_LEAN_AND_MEAN
#endif

#endif

namespace scn {
    SCN_BEGIN_NAMESPACE

    namespace detail {
        SCN_FUNC native_file_handle native_file_handle::invalid()
        {
#if SCN_WINDOWS
            return {INVALID_HANDLE_VALUE};
#else
            return {-1};
#endif
        }

        SCN_FUNC byte_mapped_file::byte_mapped_file(const char* filename)
        {
#if SCN_POSIX
            int fd = open(filename, O_RDONLY);
            if (fd == -1) {
                return;
            }

            struct stat s {};
            int status = fstat(fd, &s);
            if (status == -1) {
                close(fd);
                return;
            }
            auto size = s.st_size;

            auto ptr =
                static_cast<char*>(mmap(nullptr, static_cast<size_t>(size),
                                        PROT_READ, MAP_PRIVATE, fd, 0));
            if (ptr == MAP_FAILED) {
                close(fd);
                return;
            }

            m_file.handle = fd;
            m_map = span<char>{ptr, static_cast<size_t>(size)};
#elif SCN_WINDOWS
            auto f = ::CreateFileA(
                filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (f == INVALID_HANDLE_VALUE) {
                return;
            }

            LARGE_INTEGER _size;
            if (::GetFileSizeEx(f, &_size) == 0) {
                ::CloseHandle(f);
                return;
            }
            auto size = static_cast<size_t>(_size.QuadPart);

            auto h = ::CreateFileMappingA(
                f, nullptr, PAGE_READONLY,
#ifdef _WIN64
                static_cast<DWORD>(size >> 32ull),
#else
                DWORD{0},
#endif
                static_cast<DWORD>(size & 0xffffffffull), nullptr);
            if (h == INVALID_HANDLE_VALUE || h == nullptr) {
                ::CloseHandle(f);
                return;
            }

            auto start = ::MapViewOfFile(h, FILE_MAP_READ, 0, 0, size);
            if (!start) {
                ::CloseHandle(h);
                ::CloseHandle(f);
                return;
            }

            m_file.handle = f;
            m_map_handle.handle = h;
            m_map = span<char>{static_cast<char*>(start), size};
#else
            SCN_UNUSED(filename);
#endif
        }

        SCN_FUNC void byte_mapped_file::_destruct()
        {
#if SCN_POSIX
            munmap(m_map.data(), m_map.size());
            close(m_file.handle);
#elif SCN_WINDOWS
            ::CloseHandle(m_map_handle.handle);
            ::CloseHandle(m_file.handle);
            m_map_handle = native_file_handle::invalid();
#endif

            m_file = native_file_handle::invalid();
            m_map = span<char>{};

            SCN_ENSURE(!valid());
        }

    }  // namespace detail

    namespace detail {
        template <typename CharT>
        class basic_erased_range_impl_for_file_impl
            : public basic_erased_range_impl_for_file<CharT> {
        public:
            basic_erased_range_impl_for_file_impl(FILE* h,
                                                  file_buffering buffering)
                : m_file{h}, m_buffering{buffering}
            {
                _init();
            }

            void sync() override {}

            FILE* get_file_handle() const
            {
                return m_file;
            }

        private:
            expected<CharT> do_get_at(std::ptrdiff_t i) override
            {
                if (_should_read_more(i)) {
                    // no chars have been read
                    m_last_error = _get_more();
                    if (!m_last_error) {
                        return m_last_error;
                    }
                }
                return _get_char_at(i);
            }

            span<const CharT> do_avail_starting_at(
                std::ptrdiff_t) const override
            {
                return {};
            }

            std::ptrdiff_t do_current_index() const override
            {
                return m_last_read_index + m_chars_in_past_buffers;
            }
            bool do_is_current_at_end() const override
            {
                return _is_at_end(do_current_index());
            }

            error do_advance_current(std::ptrdiff_t i) override
            {
                return do_get_at(do_current_index() + i).error();
            }

            error _read_single();
            error _read_line();
            error _read_chars(std::size_t n);

            error _get_more();

            void _init();

            void _reclaim_buffer()
            {
                m_chars_in_past_buffers += m_last_read_index;
                m_last_read_index = 0;
            }

            span<CharT> _get_buffer()
            {
                return {&m_buffer[0], m_buffer.size()};
            }
            span<const CharT> _get_buffer() const
            {
                return {m_buffer.data(), m_buffer.size()};
            }

            span<CharT> _get_buffer_for_reading()
            {
                return _get_buffer().subspan(m_last_read_index);
            }
            span<const CharT> _get_buffer_for_reading() const
            {
                return _get_buffer().subspan(m_last_read_index);
            }

            CharT _get_char_at(std::size_t index) const
            {
                return _get_buffer()[index - m_chars_in_past_buffers];
            }
            bool _should_read_more(std::size_t index) const
            {
                return m_last_read_index < index - m_chars_in_past_buffers &&
                       !m_eof_reached;
            }
            bool _is_at_end(std::size_t index) const
            {
                return m_last_read_index == index - m_chars_in_past_buffers &&
                       m_eof_reached;
            }

            std::basic_string<CharT> m_buffer{};
            FILE* m_file;
            error m_last_error{};
            std::size_t m_chars_in_past_buffers{0};
            std::size_t m_last_read_index{0};
            file_buffering m_buffering;
            bool m_eof_reached{false};
        };

        static error _file_read_single(FILE* f, char& ch)
        {
            SCN_EXPECT(f != nullptr);
            int tmp = std::fgetc(f);
            if (tmp == EOF) {
                if (std::feof(f) != 0) {
                    return {error::end_of_range, "EOF"};
                }
                if (std::ferror(f) != 0) {
                    return {error::source_error, "fgetc error"};
                }
                return {error::unrecoverable_source_error,
                        "Unknown fgetc error"};
            }
            ch = static_cast<char>(tmp);
            return {};
        }
        static error _file_read_single(FILE* f, wchar_t& ch)
        {
            SCN_EXPECT(f != nullptr);
            wint_t tmp = std::fgetwc(f);
            if (tmp == WEOF) {
                if (std::feof(f) != 0) {
                    return {error::end_of_range, "EOF"};
                }
                if (std::ferror(f) != 0) {
                    return {error::source_error, "fgetc error"};
                }
                return {error::unrecoverable_source_error,
                        "Unknown fgetc error"};
            }
            ch = static_cast<wchar_t>(tmp);
            return {};
        }

        template <typename CharT>
        static std::pair<std::size_t, error> _file_read_multiple(
            FILE* f,
            span<CharT> buf)
        {
            SCN_EXPECT(f);
            SCN_EXPECT(!buf.empty());

            auto ret = std::fread(buf.data(), sizeof(CharT), buf.size(), f);
            if (ret < buf.size()) {
                if (std::feof(f) != 0) {
                    return {ret, {error::end_of_range, "EOF"}};
                }
                if (std::ferror(f) != 0) {
                    return {ret, error{error::source_error, "fread error"}};
                }
                return {ret, error{error::unrecoverable_source_error,
                                   "Unknown fread error"}};
            }
            return {ret, {}};
        }

        template <typename CharT>
        SCN_FUNC error
        basic_erased_range_impl_for_file_impl<CharT>::_read_single()
        {
            SCN_EXPECT(!_get_buffer_for_reading().empty());
            CharT ch;
            auto e = _file_read_single(m_file, ch);
            if (!e) {
                return e;
            }
            _get_buffer_for_reading().front() = ch;
            ++m_last_read_index;
            return {};
        }

        template <typename CharT>
        SCN_FUNC error
        basic_erased_range_impl_for_file_impl<CharT>::_read_line()
        {
            SCN_EXPECT(!_get_buffer_for_reading().empty());

            while (!_get_buffer_for_reading().empty()) {
                char ch{};
                auto e = _file_read_single(m_file, ch);
                if (!e) {
                    return e;
                }
                _get_buffer_for_reading().front() = ch;
                ++m_last_read_index;
                if (ch == detail::ascii_widen<CharT>('\n')) {
                    break;
                }
            }
            return {};
        }

        template <typename CharT>
        error basic_erased_range_impl_for_file_impl<CharT>::_read_chars(
            std::size_t n)
        {
            SCN_EXPECT(_get_buffer_for_reading().size() >= n);

            auto ret =
                _file_read_multiple(m_file, _get_buffer_for_reading().first(n));
            m_last_read_index += ret.first;
            return ret.second;
        }

        template <typename CharT>
        error basic_erased_range_impl_for_file_impl<CharT>::_get_more()
        {
            SCN_EXPECT(!m_eof_reached);

            if (_get_buffer_for_reading().empty()) {
                _reclaim_buffer();
            }

            error err{};
            if (m_buffering == file_buffering::full) {
                err = _read_chars(_get_buffer_for_reading().size());
            }
            else if (m_buffering == file_buffering::line) {
                err = _read_line();
            }
            else if (m_buffering == file_buffering::none) {
                err = _read_single();
            }

            if (!err) {
                if (err.code() == error::end_of_range) {
                    m_eof_reached = true;
                }
                else {
                    m_last_error = err;
                }
            }
            return err;
        }

        template <typename CharT>
        SCN_FUNC void basic_erased_range_impl_for_file_impl<CharT>::_init()
        {
#if SCN_POSIX
            const auto fd = ::fileno(m_file);

            struct ::stat s {};
            auto ret = ::fstat(fd, &s);
            bool is_socket = false;
            std::size_t blksize = BUFSIZ;

            if (ret == 0) {
                is_socket = S_ISSOCK(s.st_mode);
                blksize = static_cast<std::size_t>(s.st_blksize);
            }

            if (m_buffering == file_buffering::detect) {
                const bool is_tty = ::isatty(fd) == 1;
                if (is_tty || is_socket) {
                    m_buffering = file_buffering::none;
                }
                else {
                    m_buffering = file_buffering::full;
                }
            }

            m_buffer.resize(blksize / sizeof(CharT));
#elif SCN_WINDOWS
            const auto fd = ::_fileno(m_file);

            if (m_buffering == file_buffering::detect) {
                const auto is_tty = ::_isatty(fd) != 0;
                if (is_tty) {
                    m_buffering = file_buffering::none;
                }
                else {
                    m_buffering = file_buffering::full;
                }
            }

            m_buffer.resize(BUFSIZ / sizeof(CharT));
#else
            if (m_buffering == file_buffering::detect) {
                m_buffering = file_buffering::none;
            }

            m_buffer.resize(BUFSIZ / sizeof(CharT));
#endif
        }
    }  // namespace detail

    template <typename CharT>
    basic_file<CharT>::basic_file(FILE* handle, file_buffering buffering)
        : basic_erased_range<CharT>{
              detail::make_unique<
                  detail::basic_erased_range_impl_for_file_impl<CharT>>(
                  handle,
                  buffering),
              0}
    {
    }

#if SCN_INCLUDE_SOURCE_DEFINITIONS
    namespace detail {
        template class basic_erased_range_impl_for_file<char>;
        template class basic_erased_range_impl_for_file<wchar_t>;

        template class basic_erased_range_impl_for_file_impl<char>;
        template class basic_erased_range_impl_for_file_impl<wchar_t>;
    }  // namespace detail

    template class basic_file<char>;
    template class basic_file<wchar_t>;
#endif

#if 0
        static error _file_read_single(FILE* f, char& ch)
    {
        SCN_EXPECT(f != nullptr);
        int tmp = std::fgetc(f);
        if (tmp == EOF) {
            if (std::feof(f) != 0) {
                return error(error::end_of_range, "EOF");
            }
            if (std::ferror(f) != 0) {
                return error(error::source_error, "fgetc error");
            }
            return error(error::unrecoverable_source_error,
                         "Unknown fgetc error");
        }
        ch = static_cast<char>(tmp);
        return {};
    }
    static error _file_read_single(FILE* f, wchar_t& ch)
    {
        SCN_EXPECT(f != nullptr);
        wint_t tmp = std::fgetwc(f);
        if (tmp == WEOF) {
            if (std::feof(f) != 0) {
                return error(error::end_of_range, "EOF");
            }
            if (std::ferror(f) != 0) {
                return error(error::source_error, "fgetc error");
            }
            return error(error::unrecoverable_source_error,
                         "Unknown fgetc error");
        }
        ch = static_cast<wchar_t>(tmp);
        return {};
    }

    template <typename CharT>
    SCN_FUNC error basic_file<CharT>::_read_single()
    {
        SCN_EXPECT(valid());
        SCN_EXPECT(!_get_buffer_for_reading().empty());
        CharT ch;
        auto e = _file_read_single(m_file, ch);
        if (!e) {
            return e;
        }
        _get_buffer_for_reading().front() = ch;
        ++m_last_read_index;
        return {};
    }

    template <typename CharT>
    SCN_FUNC error basic_file<CharT>::_read_line()
    {
        SCN_EXPECT(valid());
        SCN_EXPECT(!_get_buffer_for_reading().empty());

        while (!_get_buffer_for_reading().empty()) {
            char ch{};
            auto e = _file_read_single(m_file, ch);
            if (!e) {
                return e;
            }
            _get_buffer_for_reading().front() = ch;
            ++m_last_read_index;
            if (ch == detail::ascii_widen<CharT>('\n')) {
                break;
            }
        }
        return {};
    }

    template <typename CharT>
    static std::pair<std::size_t, error> _file_read_multiple(FILE* f,
                                                             span<CharT> buf)
    {
        SCN_EXPECT(f);
        SCN_EXPECT(!buf.empty());

        auto ret = std::fread(buf.data(), sizeof(CharT), buf.size(), f);
        if (ret < buf.size()) {
            if (std::feof(f) != 0) {
                return {ret, {error::end_of_range, "EOF"}};
            }
            if (std::ferror(f) != 0) {
                return {ret, error{error::source_error, "fread error"}};
            }
            return {ret, error{error::unrecoverable_source_error,
                               "Unknown fread error"}};
        }
        return {ret, {}};
    }

    template <typename CharT>
    error basic_file<CharT>::_read_chars(std::size_t n)
    {
        SCN_EXPECT(valid());
        SCN_EXPECT(_get_buffer_for_reading().size() >= n);

        auto ret =
            _file_read_multiple(m_file, _get_buffer_for_reading().first(n));
        m_last_read_index += ret.first;
        return ret.second;
    }

    template <typename CharT>
    error basic_file<CharT>::_get_more()
    {
        SCN_EXPECT(valid());
        SCN_EXPECT(!m_eof_reached);

        if (_get_buffer_for_reading().empty()) {
            _reclaim_buffer();
        }

        error err{};
        if (m_buffering == file_buffering::full) {
            err = _read_chars(_get_buffer_for_reading().size());
        }
        else if (m_buffering == file_buffering::line) {
            err = _read_line();
        }
        else if (m_buffering == file_buffering::none) {
            err = _read_single();
        }

        if (!err) {
            if (err.code() == error::end_of_range) {
                m_eof_reached = true;
            }
            else {
                m_last_error = err;
            }
        }
        return err;
    }

    template <typename CharT>
    SCN_FUNC void basic_file<CharT>::_init()
    {
        SCN_EXPECT(valid());

#if SCN_POSIX
        const auto fd = ::fileno(m_file);

        struct ::stat s {};
        auto ret = ::fstat(fd, &s);
        bool is_socket = false;
        std::size_t blksize = BUFSIZ;

        if (ret == 0) {
            is_socket = S_ISSOCK(s.st_mode);
            blksize = static_cast<std::size_t>(s.st_blksize);
        }

        if (m_buffering == file_buffering::detect) {
            const bool is_tty = ::isatty(fd) == 1;
            if (is_tty || is_socket) {
                m_buffering = file_buffering::none;
            }
            else {
                m_buffering = file_buffering::full;
            }
        }

        if (m_ext_buffer.empty()) {
            m_buffer.resize(blksize / sizeof(CharT));
        }
#elif SCN_WINDOWS
        const auto fd = ::_fileno(m_file);

        if (m_buffering == file_buffering::detect) {
            const auto is_tty = ::_isatty(fd) != 0;
            if (is_tty) {
                m_buffering = file_buffering::none;
            }
            else {
                m_buffering = file_buffering::full;
            }
        }

        if (m_ext_buffer.empty()) {
            m_buffer.resize(BUFSIZ / sizeof(CharT));
        }
#else
        if (m_buffering == file_buffering::detect) {
            if (f == stdin) {
                m_buffering = file_buffering::none;
            }
            else {
                m_buffering = file_buffering::full;
            }
        }

        if (m_ext_buffer.empty()) {
            m_buffer.resize(BUFSIZ / sizeof(CharT));
        }
#endif
    }

    namespace detail {
        template <typename CharT>
        struct basic_file_iterator_access {
            using iterator = typename basic_file<CharT>::iterator;

            basic_file_iterator_access(const iterator& it) : self(it) {}

            SCN_NODISCARD basic_file<CharT>& get_file() const
            {
                SCN_EXPECT(self.m_file);
                // ew
                return *const_cast<basic_file<CharT>*>(self.m_file);
            }

            SCN_NODISCARD expected<CharT> deref() const
            {
                SCN_EXPECT(self.m_file);

                if (self.m_file->_should_read_more(self.m_current)) {
                    // no chars have been read
                    self.m_last_error = get_file()._get_more();
                    if (!self.m_last_error) {
                        return self.m_last_error;
                    }
                }
                if (!self.m_last_error) {
                    // last read failed
                    return self.m_last_error;
                }
                return get_file()._get_char_at(self.m_current);
            }

            void inc()
            {
                ++self.m_current;
            }

            SCN_NODISCARD bool eq(const iterator& o) const
            {
                if (self.m_file && (self.m_file == o.m_file || !o.m_file)) {
                    if (self.m_file->_should_read_more(self.m_current) &&
                        self.m_last_error.code() != error::end_of_range &&
                        !o.m_file) {
                        self.m_last_error = error{};
                        auto e = get_file()._get_more();
                        if (!e) {
                            self.m_last_error = e;
                            return !o.m_file || self.m_current == o.m_current ||
                                   o.m_last_error.code() == error::end_of_range;
                        }
                    }
                }

                // null file == null file
                if (!self.m_file && !o.m_file) {
                    return true;
                }
                // null file == eof file
                if (!self.m_file && o.m_file) {
                    // lhs null, rhs potentially eof
                    return o.m_last_error.code() == error::end_of_range;
                }
                // eof file == null file
                if (self.m_file && !o.m_file) {
                    // rhs null, lhs potentially eof
                    return self.m_last_error.code() == error::end_of_range;
                }
                // eof file == eof file
                if (self.m_last_error == o.m_last_error &&
                    self.m_last_error.code() == error::end_of_range) {
                    return true;
                }

                return self.m_file == o.m_file && self.m_current == o.m_current;
            }

            const iterator& self;
        };
    }  // namespace detail

    template <typename CharT>
    typename basic_file<CharT>::iterator&
    basic_file<CharT>::iterator::operator++()
    {
        SCN_EXPECT(m_file);
        detail::basic_file_iterator_access<CharT>(*this).inc();
        return *this;
    }

    template <typename CharT>
    expected<CharT> basic_file<CharT>::iterator::operator*() const
    {
        return detail::basic_file_iterator_access<CharT>(*this).deref();
    }

    template <typename CharT>
    bool basic_file<CharT>::iterator::operator==(
        const typename basic_file<CharT>::iterator& o) const
    {
        return detail::basic_file_iterator_access<CharT>(*this).eq(o);
    }

#if SCN_INCLUDE_SOURCE_DEFINITIONS
    template class basic_file<char>;
    template class basic_file<wchar_t>;
#endif

#endif

#if 0
            namespace detail {
        template <typename CharT>
        struct basic_file_iterator_access {
            using iterator = typename basic_file<CharT>::iterator;

            basic_file_iterator_access(const iterator& it) : self(it) {}

            SCN_NODISCARD expected<CharT> deref() const
            {
                SCN_EXPECT(self.m_file);

                if (self.m_file->m_buffer.empty()) {
                    // no chars have been read
                    return self.m_file->_read_single();
                }
                if (!self.m_last_error) {
                    // last read failed
                    return self.m_last_error;
                }
                return self.m_file->_get_char_at(self.m_current);
            }

            SCN_NODISCARD bool eq(const iterator& o) const
            {
                if (self.m_file && (self.m_file == o.m_file || !o.m_file)) {
                    if (self.m_file->_is_at_end(self.m_current) &&
                        self.m_last_error.code() != error::end_of_range &&
                        !o.m_file) {
                        self.m_last_error = error{};
                        auto r = self.m_file->_read_single();
                        if (!r) {
                            self.m_last_error = r.error();
                            return !o.m_file || self.m_current == o.m_current ||
                                   o.m_last_error.code() == error::end_of_range;
                        }
                    }
                }

                // null file == null file
                if (!self.m_file && !o.m_file) {
                    return true;
                }
                // null file == eof file
                if (!self.m_file && o.m_file) {
                    // lhs null, rhs potentially eof
                    return o.m_last_error.code() == error::end_of_range;
                }
                // eof file == null file
                if (self.m_file && !o.m_file) {
                    // rhs null, lhs potentially eof
                    return self.m_last_error.code() == error::end_of_range;
                }
                // eof file == eof file
                if (self.m_last_error == o.m_last_error &&
                    self.m_last_error.code() == error::end_of_range) {
                    return true;
                }

                return self.m_file == o.m_file && self.m_current == o.m_current;
            }

            const iterator& self;
        };
    }  // namespace detail

    template <>
    SCN_FUNC expected<char> basic_file<char>::iterator::operator*() const
    {
        return detail::basic_file_iterator_access<char>(*this).deref();
    }
    template <>
    SCN_FUNC expected<wchar_t> basic_file<wchar_t>::iterator::operator*() const
    {
        return detail::basic_file_iterator_access<wchar_t>(*this).deref();
    }

    template <>
    SCN_FUNC bool basic_file<char>::iterator::operator==(
        const basic_file<char>::iterator& o) const
    {
        return detail::basic_file_iterator_access<char>(*this).eq(o);
    }
    template <>
    SCN_FUNC bool basic_file<wchar_t>::iterator::operator==(
        const basic_file<wchar_t>::iterator& o) const
    {
        return detail::basic_file_iterator_access<wchar_t>(*this).eq(o);
    }

    template <>
    SCN_FUNC expected<char> file::_read_single() const
    {
        SCN_EXPECT(valid());
        int tmp = std::fgetc(m_file);
        if (tmp == EOF) {
            if (std::feof(m_file) != 0) {
                return error(error::end_of_range, "EOF");
            }
            if (std::ferror(m_file) != 0) {
                return error(error::source_error, "fgetc error");
            }
            return error(error::unrecoverable_source_error,
                         "Unknown fgetc error");
        }
        auto ch = static_cast<char>(tmp);
        m_buffer.push_back(ch);
        return ch;
    }
    template <>
    SCN_FUNC expected<wchar_t> wfile::_read_single() const
    {
        SCN_EXPECT(valid());
        wint_t tmp = std::fgetwc(m_file);
        if (tmp == WEOF) {
            if (std::feof(m_file) != 0) {
                return error(error::end_of_range, "EOF");
            }
            if (std::ferror(m_file) != 0) {
                return error(error::source_error, "fgetc error");
            }
            return error(error::unrecoverable_source_error,
                         "Unknown fgetc error");
        }
        auto ch = static_cast<wchar_t>(tmp);
        m_buffer.push_back(ch);
        return ch;
    }

    template <>
    SCN_FUNC void file::_sync_until(std::size_t pos) noexcept
    {
        for (auto it = m_buffer.rbegin();
             it != m_buffer.rend() - static_cast<std::ptrdiff_t>(pos); ++it) {
            std::ungetc(static_cast<unsigned char>(*it), m_file);
        }
    }
    template <>
    SCN_FUNC void wfile::_sync_until(std::size_t pos) noexcept
    {
        for (auto it = m_buffer.rbegin();
             it != m_buffer.rend() - static_cast<std::ptrdiff_t>(pos); ++it) {
            std::ungetwc(static_cast<wint_t>(*it), m_file);
        }
    }
#endif

    SCN_END_NAMESPACE
}  // namespace scn
