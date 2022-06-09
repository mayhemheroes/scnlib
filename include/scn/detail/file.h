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

#ifndef SCN_DETAIL_FILE_H
#define SCN_DETAIL_FILE_H

#include <cstdio>
#include <string>

#include "../util/algorithm.h"
#include "erased_range.h"
#include "range.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    namespace detail {
        struct native_file_handle {
#if SCN_WINDOWS
            using handle_type = void*;
#else
            using handle_type = int;
#endif

            static native_file_handle invalid();

            handle_type handle;
        };

        class byte_mapped_file {
        public:
            using iterator = const char*;
            using sentinel = const char*;

            byte_mapped_file() = default;
            explicit byte_mapped_file(const char* filename);

            byte_mapped_file(const byte_mapped_file&) = delete;
            byte_mapped_file& operator=(const byte_mapped_file&) = delete;

            byte_mapped_file(byte_mapped_file&& o) noexcept
                : m_map(exchange(o.m_map, span<char>{})),
                  m_file(exchange(o.m_file, native_file_handle::invalid()))
            {
#if SCN_WINDOWS
                m_map_handle =
                    exchange(o.m_map_handle, native_file_handle::invalid());
#endif
                SCN_ENSURE(!o.valid());
                SCN_ENSURE(valid());
            }
            byte_mapped_file& operator=(byte_mapped_file&& o) noexcept
            {
                if (valid()) {
                    _destruct();
                }

                m_map = exchange(o.m_map, span<char>{});
                m_file = exchange(o.m_file, native_file_handle::invalid());
#if SCN_WINDOWS
                m_map_handle =
                    exchange(o.m_map_handle, native_file_handle::invalid());
#endif

                SCN_ENSURE(!o.valid());
                SCN_ENSURE(valid());
                return *this;
            }

            ~byte_mapped_file()
            {
                if (valid()) {
                    _destruct();
                }
            }

            SCN_NODISCARD bool valid() const
            {
                return m_file.handle != native_file_handle::invalid().handle;
            }

            SCN_NODISCARD iterator begin() const
            {
                return m_map.begin();
            }
            SCN_NODISCARD sentinel end() const
            {
                return m_map.end();
            }

        protected:
            void _destruct();

            span<char> m_map{};
            native_file_handle m_file{native_file_handle::invalid().handle};
#if SCN_WINDOWS
            native_file_handle m_map_handle{
                native_file_handle::invalid().handle};
#endif
        };
    }  // namespace detail

    /**
     * Memory-mapped file range.
     * Manages the lifetime of the mapping itself.
     */
    template <typename CharT>
    class basic_mapped_file : public detail::byte_mapped_file {
    public:
        using iterator = const CharT*;
        using sentinel = const CharT*;

        /// Constructs an empty mapping
        basic_mapped_file() = default;

        /// Constructs a mapping to a filename
        explicit basic_mapped_file(const char* f) : detail::byte_mapped_file{f}
        {
        }

        SCN_NODISCARD iterator begin() const noexcept
        {
            // embrace the UB
            return reinterpret_cast<iterator>(byte_mapped_file::begin());
        }
        SCN_NODISCARD sentinel end() const noexcept
        {
            return reinterpret_cast<sentinel>(byte_mapped_file::end());
        }

        SCN_NODISCARD iterator data() const noexcept
        {
            return begin();
        }
        SCN_NODISCARD size_t size() const noexcept
        {
            return m_map.size() / sizeof(CharT);
        }

        /// Mapping data
        span<const CharT> buffer() const
        {
            return {data(), size()};
        }

#if 0
        ready_prepared_range<basic_string_view<CharT>> prepare() const& noexcept
        {
            return {{data(), size()}};
        }
        pending_prepared_range<basic_mapped_file, basic_string_view<CharT>>
        prepare() && noexcept
        {
            return {SCN_MOVE(*this)};
        }
#else
        basic_string_view<CharT> prepare() const& noexcept
        {
            return {data(), size()};
        }
#endif
    };

    using mapped_file = basic_mapped_file<char>;
    using mapped_wfile = basic_mapped_file<wchar_t>;

    enum class file_buffering {
        // Read BUFSIZ (etc) into buffer
        full,
        // Read char-by-char until line break
        line,
        // Read char-by-char
        none,

        // Detect from given file:
        // if tty -> none
        // else -> full
        detect
    };

    namespace detail {
        template <typename CharT>
        class basic_erased_range_impl_for_file
            : public basic_erased_range_impl_base<CharT> {
        public:
            basic_erased_range_impl_for_file() = default;

            virtual void sync() = 0;

            virtual FILE* get_file_handle() const = 0;
        };
    }  // namespace detail

    template <typename CharT>
    class basic_file : public basic_erased_range<CharT> {
    public:
        basic_file() = default;
        basic_file(FILE* handle,
                   file_buffering buffering = file_buffering::detect);

        void sync()
        {
            SCN_EXPECT(_get_impl_ptr());
            _get_impl_ptr()->sync();
        }

        FILE* handle() const
        {
            auto p = _get_impl_ptr();
            if (!p) {
                return nullptr;
            }
            return p->get_file_handle();
        }

    private:
        using file_impl_type = detail::basic_erased_range_impl_for_file<CharT>;

        file_impl_type* _get_impl_ptr()
        {
            return static_cast<file_impl_type*>(
                basic_erased_range<CharT>::_get_impl().get());
        }
        const file_impl_type* _get_impl_ptr() const
        {
            return static_cast<const file_impl_type*>(
                basic_erased_range<CharT>::_get_impl().get());
        }
    };

    using file = basic_file<char>;
    using wfile = basic_file<wchar_t>;

    extern template file::basic_file(FILE*, file_buffering);
    extern template wfile::basic_file(FILE*, file_buffering);

    SCN_CLANG_PUSH
    SCN_CLANG_IGNORE("-Wexit-time-destructors")

    // Avoid documentation issues: without this, Doxygen will think
    // SCN_CLANG_PUSH is a part of the stdin_range declaration
    namespace dummy {
    }

    /**
     * Get a reference to the global stdin range
     */
    template <typename CharT>
    basic_file<CharT>& stdin_range()
    {
        static auto f = basic_file<CharT>{stdin};
        return f;
    }
    /**
     * Get a reference to the global `char`-oriented stdin range
     */
    inline file& cstdin()
    {
        return stdin_range<char>();
    }
    /**
     * Get a reference to the global `wchar_t`-oriented stdin range
     */
    inline wfile& wcstdin()
    {
        return stdin_range<wchar_t>();
    }
    SCN_CLANG_POP

#if 0

namespace detail {
            template <typename CharT>
            struct basic_file_access;
            template <typename CharT>
            struct basic_file_iterator_access;
        }  // namespace detail

        /**
         * Range mapping to a C FILE*.
         * Not copyable or reconstructible.
         */
        template <typename CharT>
        class basic_file {
            friend struct detail::basic_file_access<CharT>;
            friend struct detail::basic_file_iterator_access<CharT>;

        public:
            class iterator {
                friend struct detail::basic_file_iterator_access<CharT>;

            public:
                using char_type = CharT;
                using value_type = expected<CharT>;
                using reference = value_type;
                using pointer = value_type*;
                using difference_type = std::ptrdiff_t;
                using iterator_category = std::bidirectional_iterator_tag;
                using file_type = basic_file<CharT>;

                iterator() = default;

                expected<CharT> operator*() const;

                iterator& operator++()
                {
                    SCN_EXPECT(m_file);
                    ++m_current;
                    return *this;
                }
                iterator operator++(int)
                {
                    iterator tmp(*this);
                    operator++();
                    return tmp;
                }

                iterator& operator--()
                {
                    SCN_EXPECT(m_file);
                    SCN_EXPECT(m_current > 0);

                    m_last_error = error{};
                    --m_current;

                    return *this;
                }
                iterator operator--(int)
                {
                    iterator tmp(*this);
                    operator--();
                    return tmp;
                }

                bool operator==(const iterator& o) const;

                bool operator!=(const iterator& o) const
                {
                    return !operator==(o);
                }

                bool operator<(const iterator& o) const
                {
                    // any valid iterator is before eof and null
                    if (!m_file) {
                        return !o.m_file;
                    }
                    if (!o.m_file) {
                        return !m_file;
                    }
                    SCN_EXPECT(m_file == o.m_file);
                    return m_current < o.m_current;
                }
                bool operator>(const iterator& o) const
                {
                    return o.operator<(*this);
                }
                bool operator<=(const iterator& o) const
                {
                    return !operator>(o);
                }
                bool operator>=(const iterator& o) const
                {
                    return !operator<(o);
                }

                void reset_begin_iterator() const noexcept
                {
                    m_current = 0;
                }

            private:
                friend class basic_file;

                iterator(const file_type& f, size_t i)
                    : m_file{std::addressof(f)}, m_current{i}
                {
                }

                mutable error m_last_error{};
                const file_type* m_file{nullptr};
                mutable size_t m_current{0};
            };

            using sentinel = iterator;
            using char_type = CharT;

            /**
             * Construct an empty file.
             * Reading not possible: valid() is `false`
             */
            basic_file() = default;
            /**
             * Construct from a FILE*.
             * Must be a valid handle that can be read from.
             */
            basic_file(FILE* f) : m_file{f} {}

            basic_file(const basic_file&) = delete;
            basic_file& operator=(const basic_file&) = delete;

            basic_file(basic_file&& o) noexcept
                : m_buffer(detail::exchange(o.m_buffer, {})),
                  m_file(detail::exchange(o.m_file, nullptr))
            {
            }
            basic_file& operator=(basic_file&& o) noexcept
            {
                if (valid()) {
                    sync();
                }
                m_buffer = detail::exchange(o.m_buffer, {});
                m_file = detail::exchange(o.m_file, nullptr);
                return *this;
            }

            ~basic_file()
            {
                if (valid()) {
                    _sync_all();
                }
            }

            /**
             * Get the FILE* for this range.
             * Only use this handle for reading sync() has been called and no
             * reading operations have taken place after that.
             *
             * \see sync
             */
            FILE* handle() const
            {
                return m_file;
            }

            /**
             * Reset the file handle.
             * Calls sync(), if necessary, before resetting.
             * @return The old handle
             */
            FILE* set_handle(FILE* f, bool allow_sync = true) noexcept
            {
                auto old = m_file;
                if (old && allow_sync) {
                    sync();
                }
                m_file = f;
                return old;
            }

            /// Whether the file has been opened
            constexpr bool valid() const noexcept
            {
                return m_file != nullptr;
            }

            /**
             * Synchronizes this file with the underlying FILE*.
             * Invalidates all non-end iterators.
             * File must be open.
             *
             * Necessary for mixing-and-matching scnlib and <cstdio>:
             * \code{.cpp}
             * scn::scan(file, ...);
             * file.sync();
             * std::fscanf(file.handle(), ...);
             * \endcode
             *
             * Necessary for synchronizing result objects:
             * \code{.cpp}
             * auto result = scn::scan(file, ...);
             * // only result.range() can now be used for scanning
             * result = scn::scan(result.range(), ...);
             * // .sync() allows the original file to also be used
             * file.sync();
             * result = scn::scan(file, ...);
             * \endcode
             */
            void sync() noexcept
            {
                _sync_all();
                m_buffer.clear();
            }

            iterator begin() const noexcept
            {
                return {*this, 0};
            }
            sentinel end() const noexcept
            {
                return {};
            }

            span<const CharT> get_buffer(iterator it,
                                         size_t max_size) const noexcept
            {
                if (!it.m_file) {
                    return {};
                }
                const auto begin =
                    m_buffer.begin() + static_cast<std::ptrdiff_t>(it.m_current);
                const auto end_diff = detail::min(
                    max_size,
                    static_cast<size_t>(ranges::distance(begin, m_buffer.end())));
                return {begin, begin + static_cast<std::ptrdiff_t>(end_diff)};
            }

            detail::range_wrapper<basic_file<CharT>&> wrap() &
            {
                return {*this};
            }
            detail::range_wrapper<basic_file<CharT>> wrap() &&
            {
                return {SCN_MOVE(*this)};
            }

        private:
            friend class iterator;

            expected<CharT> _read_single() const;

            void _sync_all() noexcept
            {
                _sync_until(m_buffer.size());
            }
            void _sync_until(size_t pos) noexcept;

            CharT _get_char_at(size_t i) const
            {
                SCN_EXPECT(valid());
                SCN_EXPECT(i < m_buffer.size());
                return m_buffer[i];
            }

            bool _is_at_end(size_t i) const
            {
                SCN_EXPECT(valid());
                return i >= m_buffer.size();
            }

            mutable std::basic_string<CharT> m_buffer{};
            FILE* m_file{nullptr};
        };

        using file = basic_file<char>;
        using wfile = basic_file<wchar_t>;

        template <>
        expected<char> file::iterator::operator*() const;
        template <>
        expected<wchar_t> wfile::iterator::operator*() const;
        template <>
        bool file::iterator::operator==(const file::iterator&) const;
        template <>
        bool wfile::iterator::operator==(const wfile::iterator&) const;

        template <>
        expected<char> file::_read_single() const;
        template <>
        expected<wchar_t> wfile::_read_single() const;
        template <>
        void file::_sync_until(size_t) noexcept;
        template <>
        void wfile::_sync_until(size_t) noexcept;

        /**
         * A child class for basic_file, handling fopen, fclose, and lifetimes with
         * RAII.
         */
        template <typename CharT>
        class basic_owning_file : public basic_file<CharT> {
        public:
            using char_type = CharT;

            /// Open an empty file
            basic_owning_file() = default;
            /// Open a file, with fopen arguments
            basic_owning_file(const char* f, const char* mode)
                : basic_file<CharT>(std::fopen(f, mode))
            {
            }

            /// Steal ownership of a FILE*
            explicit basic_owning_file(FILE* f) : basic_file<CharT>(f) {}

            ~basic_owning_file()
            {
                if (is_open()) {
                    close();
                }
            }

            /// fopen
            bool open(const char* f, const char* mode)
            {
                SCN_EXPECT(!is_open());

                auto h = std::fopen(f, mode);
                if (!h) {
                    return false;
                }

                const bool is_wide = sizeof(CharT) > 1;
                auto ret = std::fwide(h, is_wide ? 1 : -1);
                if ((is_wide && ret > 0) || (!is_wide && ret < 0) || ret == 0) {
                    this->set_handle(h);
                    return true;
                }
                return false;
            }
            /// Steal ownership
            bool open(FILE* f)
            {
                SCN_EXPECT(!is_open());
                if (std::ferror(f) != 0) {
                    return false;
                }
                this->set_handle(f);
                return true;
            }

            /// Close file
            void close()
            {
                SCN_EXPECT(is_open());
                this->sync();
                std::fclose(this->handle());
                this->set_handle(nullptr, false);
            }

            /// Is the file open
            SCN_NODISCARD bool is_open() const
            {
                return this->valid();
            }
        };

        using owning_file = basic_owning_file<char>;
        using owning_wfile = basic_owning_file<wchar_t>;

        SCN_CLANG_PUSH
        SCN_CLANG_IGNORE("-Wexit-time-destructors")

        // Avoid documentation issues: without this, Doxygen will think
        // SCN_CLANG_PUSH is a part of the stdin_range declaration
        namespace dummy {
        }

        /**
         * Get a reference to the global stdin range
         */
        template <typename CharT>
        basic_file<CharT>& stdin_range()
        {
            static auto f = basic_file<CharT>{stdin};
            return f;
        }
        /**
         * Get a reference to the global `char`-oriented stdin range
         */
        inline file& cstdin()
        {
            return stdin_range<char>();
        }
        /**
         * Get a reference to the global `wchar_t`-oriented stdin range
         */
        inline wfile& wcstdin()
        {
            return stdin_range<wchar_t>();
        }
        SCN_CLANG_POP

#endif

#if 0

        namespace detail {
        template <typename CharT>
        struct basic_file_iterator_access;
    }

    enum class file_buffering {
        // Read BUFSIZ (etc) into buffer
        full,
        // Read char-by-char until line break
        line,
        // Read char-by-char
        none,

        // Detect from given file:
        // if tty -> none
        // else -> full
        detect
    };

    template <typename CharT>
    class basic_file {
        friend struct detail::basic_file_iterator_access<CharT>;

    public:
        class iterator {
            friend struct detail::basic_file_iterator_access<CharT>;

        public:
            using char_type = CharT;
            using value_type = expected<CharT>;
            using reference = value_type;
            using pointer = value_type*;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::bidirectional_iterator_tag;
            using file_type = basic_file<CharT>;

            iterator() = default;

            expected<CharT> operator*() const;

            iterator& operator++();

            iterator operator++(int)
            {
                iterator tmp(*this);
                operator++();
                return tmp;
            }

            iterator& operator--()
            {
                SCN_EXPECT(m_file);
                SCN_EXPECT(m_current > 0);

                m_last_error = error{};
                --m_current;

                return *this;
            }
            iterator operator--(int)
            {
                iterator tmp(*this);
                operator--();
                return tmp;
            }

            bool operator==(const iterator& o) const;

            bool operator!=(const iterator& o) const
            {
                return !operator==(o);
            }

            bool operator<(const iterator& o) const
            {
                // any valid iterator is before eof and null
                if (!m_file) {
                    return !o.m_file;
                }
                if (!o.m_file) {
                    return !m_file;
                }
                SCN_EXPECT(m_file == o.m_file);
                return m_current < o.m_current;
            }
            bool operator>(const iterator& o) const
            {
                return o.operator<(*this);
            }
            bool operator<=(const iterator& o) const
            {
                return !operator>(o);
            }
            bool operator>=(const iterator& o) const
            {
                return !operator<(o);
            }

        private:
            friend class basic_file;

            iterator(const file_type& f, size_t i)
                : m_file{std::addressof(f)}, m_current{i}
            {
            }

            mutable error m_last_error{};
            const file_type* m_file{nullptr};
            mutable size_t m_current{0};
        };

        using sentinel = iterator;
        using char_type = CharT;

        basic_file() = default;

        explicit basic_file(FILE* f,
                            file_buffering buffering = file_buffering::detect,
                            span<CharT> ext_buffer = span<CharT>{})
            : m_file(f), m_ext_buffer(ext_buffer), m_buffering(buffering)
        {
            _init();
        }

        basic_file(const basic_file&) = delete;
        basic_file& operator=(const basic_file&) = delete;

        basic_file(basic_file&&) = default;
        basic_file& operator=(basic_file&&) = default;

        ~basic_file() = default;

        FILE* handle() const
        {
            return m_file;
        }
        void set_handle(FILE* h)
        {
            m_file = h;
        }

        void sync() {}

        constexpr bool valid() const noexcept
        {
            return m_file != nullptr;
        }

        iterator begin() const
        {
            return {*this, 0};
        }
        sentinel end() const noexcept
        {
            return {};
        }

#if 0
        pending_prepared_range<basic_erased_range<CharT>,
                               basic_erased_view<CharT>>
        prepare()
        {
            return {erase_range(*this)};
        }
#endif

    private:
        error _read_single();
        error _read_line();
        error _read_chars(std::size_t);

        void _reclaim_buffer()
        {
            m_chars_in_past_buffers += m_last_read_index;
            m_last_read_index = 0;
        }

        error _get_more();

        span<CharT> _get_buffer()
        {
            if (m_ext_buffer.empty()) {
                return {&m_buffer[0], m_buffer.size()};
            }
            return m_ext_buffer;
        }
        span<const CharT> _get_buffer() const
        {
            if (m_ext_buffer.empty()) {
                return {m_buffer.data(), m_buffer.size()};
            }
            return m_ext_buffer.as_const();
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

        void _init();

        FILE* m_file{nullptr};
        std::basic_string<CharT> m_buffer{};
        span<CharT> m_ext_buffer{};
        error m_last_error{};
        std::size_t m_chars_in_past_buffers{0};
        std::size_t m_last_read_index{0};
        file_buffering m_buffering{file_buffering::none};
        bool m_eof_reached{false};
    };

    using file = basic_file<char>;
    using wfile = basic_file<wchar_t>;

    extern template error basic_file<char>::_read_single();
    extern template error basic_file<wchar_t>::_read_single();
    extern template error basic_file<char>::_read_line();
    extern template error basic_file<wchar_t>::_read_line();
    extern template error basic_file<char>::_read_chars(std::size_t);
    extern template error basic_file<wchar_t>::_read_chars(std::size_t);
    extern template error basic_file<char>::_get_more();
    extern template error basic_file<wchar_t>::_get_more();
    extern template void basic_file<char>::_init();
    extern template void basic_file<wchar_t>::_init();

    extern template expected<char> basic_file<char>::iterator::operator*()
        const;
    extern template expected<wchar_t> basic_file<wchar_t>::iterator::operator*()
        const;
    extern template basic_file<char>::iterator&
    basic_file<char>::iterator::operator++();
    extern template basic_file<wchar_t>::iterator&
    basic_file<wchar_t>::iterator::operator++();
    extern template bool basic_file<char>::iterator::operator==(
        const basic_file<char>::iterator&) const;
    extern template bool basic_file<wchar_t>::iterator::operator==(
        const basic_file<wchar_t>::iterator&) const;

    /**
     * A child class for basic_file, handling fopen, fclose, and lifetimes with
     * RAII.
     */
    template <typename CharT>
    class basic_owning_file : public basic_file<CharT> {
    public:
        using char_type = CharT;

        /// Open an empty file
        basic_owning_file() = default;
        /// Open a file, with fopen arguments
        basic_owning_file(const char* f, const char* mode)
            : basic_file<CharT>(std::fopen(f, mode))
        {
        }

        /// Steal ownership of a FILE*
        explicit basic_owning_file(FILE* f) : basic_file<CharT>(f) {}

        ~basic_owning_file()
        {
            if (is_open()) {
                close();
            }
        }

        /// fopen
        bool open(const char* f, const char* mode)
        {
            SCN_EXPECT(!is_open());

            auto h = std::fopen(f, mode);
            if (!h) {
                return false;
            }

            const bool is_wide = sizeof(CharT) > 1;
            auto ret = std::fwide(h, is_wide ? 1 : -1);
            if ((is_wide && ret > 0) || (!is_wide && ret < 0) || ret == 0) {
                this->set_handle(h);
                return true;
            }
            return false;
        }
        /// Steal ownership
        bool open(FILE* f)
        {
            SCN_EXPECT(!is_open());
            if (std::ferror(f) != 0) {
                return false;
            }
            this->set_handle(f);
            return true;
        }

        /// Close file
        void close()
        {
            SCN_EXPECT(is_open());
            this->sync();
            std::fclose(this->handle());
            this->set_handle(nullptr);
        }

        /// Is the file open
        SCN_NODISCARD bool is_open() const
        {
            return this->valid();
        }
    };

    using owning_file = basic_owning_file<char>;
    using owning_wfile = basic_owning_file<wchar_t>;

    SCN_CLANG_PUSH
    SCN_CLANG_IGNORE("-Wexit-time-destructors")

    // Avoid documentation issues: without this, Doxygen will think
    // SCN_CLANG_PUSH is a part of the stdin_range declaration
    namespace dummy {
    }

    /**
     * Get a reference to the global stdin range
     */
    template <typename CharT>
    basic_file<CharT>& stdin_range()
    {
        static auto f = basic_file<CharT>{stdin};
        return f;
    }
    /**
     * Get a reference to the global `char`-oriented stdin range
     */
    inline file& cstdin()
    {
        return stdin_range<char>();
    }
    /**
     * Get a reference to the global `wchar_t`-oriented stdin range
     */
    inline wfile& wcstdin()
    {
        return stdin_range<wchar_t>();
    }
    SCN_CLANG_POP

#endif

#if 0

    namespace detail {
        template <typename CharT>
        struct basic_file_access;
        template <typename CharT>
        struct basic_file_iterator_access;
    }  // namespace detail

    /**
     * Range mapping to a C FILE*.
     * Not copyable or reconstructible.
     */
    template <typename CharT>
    class basic_file {
        friend struct detail::basic_file_access<CharT>;
        friend struct detail::basic_file_iterator_access<CharT>;

    public:
        class iterator {
            friend struct detail::basic_file_iterator_access<CharT>;

        public:
            using char_type = CharT;
            using value_type = expected<CharT>;
            using reference = value_type;
            using pointer = value_type*;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::bidirectional_iterator_tag;
            using file_type = basic_file<CharT>;

            iterator() = default;

            expected<CharT> operator*() const;

            iterator& operator++()
            {
                SCN_EXPECT(m_file);
                ++m_current;
                return *this;
            }
            iterator operator++(int)
            {
                iterator tmp(*this);
                operator++();
                return tmp;
            }

            iterator& operator--()
            {
                SCN_EXPECT(m_file);
                SCN_EXPECT(m_current > 0);

                m_last_error = error{};
                --m_current;

                return *this;
            }
            iterator operator--(int)
            {
                iterator tmp(*this);
                operator--();
                return tmp;
            }

            bool operator==(const iterator& o) const;

            bool operator!=(const iterator& o) const
            {
                return !operator==(o);
            }

            bool operator<(const iterator& o) const
            {
                // any valid iterator is before eof and null
                if (!m_file) {
                    return !o.m_file;
                }
                if (!o.m_file) {
                    return !m_file;
                }
                SCN_EXPECT(m_file == o.m_file);
                return m_current < o.m_current;
            }
            bool operator>(const iterator& o) const
            {
                return o.operator<(*this);
            }
            bool operator<=(const iterator& o) const
            {
                return !operator>(o);
            }
            bool operator>=(const iterator& o) const
            {
                return !operator<(o);
            }

            void reset_begin_iterator() const noexcept
            {
                m_current = 0;
            }

        private:
            friend class basic_file;

            iterator(const file_type& f, size_t i)
                : m_file{std::addressof(f)}, m_current{i}
            {
            }

            mutable error m_last_error{};
            const file_type* m_file{nullptr};
            mutable size_t m_current{0};
        };

        using sentinel = iterator;
        using char_type = CharT;

        /**
         * Construct an empty file.
         * Reading not possible: valid() is `false`
         */
        basic_file() = default;
        /**
         * Construct from a FILE*.
         * Must be a valid handle that can be read from.
         */
        basic_file(FILE* f) : m_file{f} {}

        basic_file(const basic_file&) = delete;
        basic_file& operator=(const basic_file&) = delete;

        basic_file(basic_file&& o) noexcept
            : m_buffer(detail::exchange(o.m_buffer, {})),
              m_file(detail::exchange(o.m_file, nullptr))
        {
        }
        basic_file& operator=(basic_file&& o) noexcept
        {
            if (valid()) {
                sync();
            }
            m_buffer = detail::exchange(o.m_buffer, {});
            m_file = detail::exchange(o.m_file, nullptr);
            return *this;
        }

        ~basic_file()
        {
            if (valid()) {
                _sync_all();
            }
        }

        /**
         * Get the FILE* for this range.
         * Only use this handle for reading sync() has been called and no
         * reading operations have taken place after that.
         *
         * \see sync
         */
        FILE* handle() const
        {
            return m_file;
        }

        /**
         * Reset the file handle.
         * Calls sync(), if necessary, before resetting.
         * @return The old handle
         */
        FILE* set_handle(FILE* f, bool allow_sync = true) noexcept
        {
            auto old = m_file;
            if (old && allow_sync) {
                sync();
            }
            m_file = f;
            return old;
        }

        /// Whether the file has been opened
        constexpr bool valid() const noexcept
        {
            return m_file != nullptr;
        }

        /**
         * Synchronizes this file with the underlying FILE*.
         * Invalidates all non-end iterators.
         * File must be open.
         *
         * Necessary for mixing-and-matching scnlib and <cstdio>:
         * \code{.cpp}
         * scn::scan(file, ...);
         * file.sync();
         * std::fscanf(file.handle(), ...);
         * \endcode
         *
         * Necessary for synchronizing result objects:
         * \code{.cpp}
         * auto result = scn::scan(file, ...);
         * // only result.range() can now be used for scanning
         * result = scn::scan(result.range(), ...);
         * // .sync() allows the original file to also be used
         * file.sync();
         * result = scn::scan(file, ...);
         * \endcode
         */
        void sync() noexcept
        {
            _sync_all();
            m_buffer.clear();
        }

        iterator begin() const noexcept
        {
            return {*this, 0};
        }
        sentinel end() const noexcept
        {
            return {};
        }

        span<const CharT> get_buffer(iterator it,
                                     size_t max_size) const noexcept
        {
            if (!it.m_file) {
                return {};
            }
            const auto begin =
                m_buffer.begin() + static_cast<std::ptrdiff_t>(it.m_current);
            const auto end_diff = detail::min(
                max_size,
                static_cast<size_t>(ranges::distance(begin, m_buffer.end())));
            return {begin, begin + static_cast<std::ptrdiff_t>(end_diff)};
        }

        detail::range_wrapper<basic_file<CharT>&> wrap() &
        {
            return {*this};
        }
        detail::range_wrapper<basic_file<CharT>> wrap() &&
        {
            return {SCN_MOVE(*this)};
        }

    private:
        friend class iterator;

        expected<CharT> _read_single() const;

        void _sync_all() noexcept
        {
            _sync_until(m_buffer.size());
        }
        void _sync_until(size_t pos) noexcept;

        CharT _get_char_at(size_t i) const
        {
            SCN_EXPECT(valid());
            SCN_EXPECT(i < m_buffer.size());
            return m_buffer[i];
        }

        bool _is_at_end(size_t i) const
        {
            SCN_EXPECT(valid());
            return i >= m_buffer.size();
        }

        mutable std::basic_string<CharT> m_buffer{};
        FILE* m_file{nullptr};
    };

    using file = basic_file<char>;
    using wfile = basic_file<wchar_t>;

    template <>
    expected<char> file::iterator::operator*() const;
    template <>
    expected<wchar_t> wfile::iterator::operator*() const;
    template <>
    bool file::iterator::operator==(const file::iterator&) const;
    template <>
    bool wfile::iterator::operator==(const wfile::iterator&) const;

    template <>
    expected<char> file::_read_single() const;
    template <>
    expected<wchar_t> wfile::_read_single() const;
    template <>
    void file::_sync_until(size_t) noexcept;
    template <>
    void wfile::_sync_until(size_t) noexcept;

    /**
     * A child class for basic_file, handling fopen, fclose, and lifetimes with
     * RAII.
     */
    template <typename CharT>
    class basic_owning_file : public basic_file<CharT> {
    public:
        using char_type = CharT;

        /// Open an empty file
        basic_owning_file() = default;
        /// Open a file, with fopen arguments
        basic_owning_file(const char* f, const char* mode)
            : basic_file<CharT>(std::fopen(f, mode))
        {
        }

        /// Steal ownership of a FILE*
        explicit basic_owning_file(FILE* f) : basic_file<CharT>(f) {}

        ~basic_owning_file()
        {
            if (is_open()) {
                close();
            }
        }

        /// fopen
        bool open(const char* f, const char* mode)
        {
            SCN_EXPECT(!is_open());

            auto h = std::fopen(f, mode);
            if (!h) {
                return false;
            }

            const bool is_wide = sizeof(CharT) > 1;
            auto ret = std::fwide(h, is_wide ? 1 : -1);
            if ((is_wide && ret > 0) || (!is_wide && ret < 0) || ret == 0) {
                this->set_handle(h);
                return true;
            }
            return false;
        }
        /// Steal ownership
        bool open(FILE* f)
        {
            SCN_EXPECT(!is_open());
            if (std::ferror(f) != 0) {
                return false;
            }
            this->set_handle(f);
            return true;
        }

        /// Close file
        void close()
        {
            SCN_EXPECT(is_open());
            this->sync();
            std::fclose(this->handle());
            this->set_handle(nullptr, false);
        }

        /// Is the file open
        SCN_NODISCARD bool is_open() const
        {
            return this->valid();
        }
    };

    using owning_file = basic_owning_file<char>;
    using owning_wfile = basic_owning_file<wchar_t>;

    SCN_CLANG_PUSH
    SCN_CLANG_IGNORE("-Wexit-time-destructors")

    // Avoid documentation issues: without this, Doxygen will think
    // SCN_CLANG_PUSH is a part of the stdin_range declaration
    namespace dummy {
    }

    /**
     * Get a reference to the global stdin range
     */
    template <typename CharT>
    basic_file<CharT>& stdin_range()
    {
        static auto f = basic_file<CharT>{stdin};
        return f;
    }
    /**
     * Get a reference to the global `char`-oriented stdin range
     */
    inline file& cstdin()
    {
        return stdin_range<char>();
    }
    /**
     * Get a reference to the global `wchar_t`-oriented stdin range
     */
    inline wfile& wcstdin()
    {
        return stdin_range<wchar_t>();
    }
    SCN_CLANG_POP

#endif

    SCN_END_NAMESPACE
}  // namespace scn

#if defined(SCN_HEADER_ONLY) && SCN_HEADER_ONLY && !defined(SCN_FILE_CPP)
#include "file.cpp"
#endif

#endif  // SCN_DETAIL_FILE_H
