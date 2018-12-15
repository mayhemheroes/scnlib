// Copyright 2017-2018 Elias Kosunen
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

#include <benchmark/benchmark.h>
#include <scn/scn.h>
#include <cctype>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>

static std::string generate_data(size_t len)
{
    static const std::vector<char> chars = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',  'A',  'B',
        'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',  'M',  'N',
        'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',  'Y',  'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',  'k',  'l',
        'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',  'w',  'x',
        'y', 'z', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\n', '\n', '\t'};
    std::default_random_engine rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, static_cast<int>(chars.size() - 1));

    std::string data;
    for (std::size_t i = 0; i < len; ++i) {
        data.push_back(chars[static_cast<size_t>(dist(rng))]);
    }
    return data;
}
template <typename Int = int>
static std::string generate_int_data(size_t n)
{
    std::default_random_engine rng(std::random_device{}());
    std::uniform_int_distribution<Int> dist(std::numeric_limits<Int>::min(),
                                            std::numeric_limits<Int>::max());

    std::ostringstream oss;
    for (size_t i = 0; i < n; ++i) {
        oss << dist(rng) << ' ';
    }
    return oss.str();
}

template <typename Int>
static void scanint_scn(benchmark::State& state)
{
    auto data = generate_int_data(static_cast<size_t>(state.range(0)));
    auto stream = scn::make_stream(data);
    int i{};
    for (auto _ : state) {
        auto e = scn::scan(stream, "{}", i);

        benchmark::DoNotOptimize(i);
        if (!e) {
            if (e.error() == scn::error::end_of_stream) {
                state.PauseTiming();
                data = generate_int_data(static_cast<size_t>(state.range(0)));
                stream = scn::make_stream(data);
                state.ResumeTiming();
            }
            else {
                state.SkipWithError("Benchmark errored");
                break;
            }
        }
    }
    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations() * sizeof(int)));
}
BENCHMARK_TEMPLATE(scanint_scn, int)->Arg(2 << 15);
BENCHMARK_TEMPLATE(scanint_scn, long long)->Arg(2 << 15);
BENCHMARK_TEMPLATE(scanint_scn, unsigned)->Arg(2 << 15);

template <typename Int>
static void scanint_sstream(benchmark::State& state)
{
    auto data = generate_int_data(static_cast<size_t>(state.range(0)));
    auto stream = std::istringstream(data);
    int i{};
    for (auto _ : state) {
        stream >> i;

        benchmark::DoNotOptimize(i);
        if (stream.eof()) {
            state.PauseTiming();
            data = generate_int_data(static_cast<size_t>(state.range(0)));
            stream = std::istringstream(data);
            state.ResumeTiming();
            continue;
        }
        if (stream.fail()) {
            state.SkipWithError("Benchmark errored");
            break;
        }
    }
    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations() * sizeof(int)));
}
BENCHMARK_TEMPLATE(scanint_sstream, int)->Arg(2 << 15);
BENCHMARK_TEMPLATE(scanint_sstream, long long)->Arg(2 << 15);
BENCHMARK_TEMPLATE(scanint_sstream, unsigned)->Arg(2 << 15);

BENCHMARK_MAIN();
