#!/bin/sh

previous_dir=$(pwd)

base_dir=$(dirname "$0")

cd "$base_dir" || exit 1
rm -rf xed-build
mkdir xed-build
# shellcheck disable=SC2164
cd xed-build
git clone https://github.com/intelxed/mbuild && cd mbuild && git checkout 7c4497f41f576b43a80bb0f8d8452bbcfd58b6e2 && cd ..
git clone https://github.com/intelxed/xed && cd xed && git checkout 1bdc793f5f64cf207f6776f4c0e442e39fa47903 && cd ..
git clone https://github.com/ctchou/xed_utils && cd xed_utils && git checkout d54a280ea1e1d3ea50ecd865d999b929d4f3c656 && cd ..
mkdir build
# shellcheck disable=SC2164
cd build
../xed/mfile.py just-prep
../xed_utils/xed_db.py -j test.json

mv test.json "$base_dir/"
# shellcheck disable=SC2164
cd "$base_dir"
cargo run --release

cd "$previous_dir" || exit
