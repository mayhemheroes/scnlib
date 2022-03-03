#!/usr/bin/env bash

self=$0
repo_dir=$1
fuzzer=$2

if [[ $fuzzer = "roundtrip" ]]; then
  data="int"
elif [[ $fuzzer = "getline" ]]; then
  data="string"
else
  data=$fuzzer
fi

data_dir=$repo_dir/test/fuzz
script_dir=$(dirname "$(readlink -f "$self")")

mkdir "$script_dir/corpus-$fuzzer"
"$script_dir/fuzz-$fuzzer" -dict="$data_dir/dict-$data.txt" \
  "$script_dir/corpus-$fuzzer" "$data_dir/seed-corpora/$data" \
  "${@:3}"
