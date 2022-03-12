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

#ifndef SCN_DETAIL_ERASED_RANGE_H
#define SCN_DETAIL_ERASED_RANGE_H

#include "../ranges/ranges.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    namespace detail {
        template <typename CharT>
        class basic_erased_range_impl_base {
        public:
            basic_erased_range_impl_base() = default;

            expected<CharT> get_next();

        protected:
            virtual ~basic_erased_range_impl_base() = default;
        };
    }

    template <typename CharT>
    class basic_erased_range {

    };

    SCN_END_NAMESPACE
}  // namespace scn

#endif
