#!/bin/bash
#
# Test runner for length disassembler
#
# This script runs the LengthDisassemblerExample tool on several different test sets, namely zydis and radare2s
#
# - Zydis' test suite mainly focuses on edge cases
# - Radare focuses on real-world instructions
#
# - The last test is a collection of all instructions, that an arbitrary version of rust-analyzer (hosted in Arch Linux repositories), uses.
#   This is basically the real real-world test set, that features instructions that most x86 CPUs need to support.
#
# Because this tool imports test sets from other projects, it is subject to their licenses.
# This shell script is not subject to any restrictions, coming from the original license (../LICENSE), that conflict with the other licenses.
# The other licenses are available in the ./Licenses directory.

base="$(dirname $0)"
runner="$1"

fails=0
tests_completed=0

function run_test_set() {
	tests_count=$(echo $2 | wc -l)
	echo $2 | "$runner" "$1"
	status=$?
	if [ $status -ne 0 ]; then
		>&2 echo "Failed to process: $line"
		fails=$((fails+$status))
	fi
	tests_completed=$(($tests_completed+$tests_count-$status))
}

IFS='\n'

## Imported instructions

run_test_set 64 $(cat "$base/TestCases/rust-analyzer.txt")

## Radare (License at ./Licenses/LICENSE.radare2)

run_test_set 64 $(curl -s https://raw.githubusercontent.com/radareorg/radare2/a664277a3536246b3d1ff56675f99fdd354e10b8/test/db/asm/x86_64 \
	| awk -F'#' '{ print $1; }' | awk '{ print $NF; }' | grep -E '^[0-9a-fA-F]+$')
run_test_set 32 $(curl -s https://raw.githubusercontent.com/radareorg/radare2/a664277a3536246b3d1ff56675f99fdd354e10b8/test/db/asm/x86_32 \
	| awk -F'#' '{ print $1; }' | awk '{ print $NF; }' | grep -E '^[0-9a-fA-F]+$')
run_test_set 16 $(curl -s https://raw.githubusercontent.com/radareorg/radare2/a664277a3536246b3d1ff56675f99fdd354e10b8/test/db/asm/x86_16 \
	| awk -F'#' '{ print $1; }' | awk '{ print $NF; }' | grep -E '^[0-9a-fA-F]+$')

## Zydis (License at ./Licenses/LICENSE.zydis)

temp_dir=$(mktemp -d)
old_cwd=$(pwd)

cd "$temp_dir"
git clone https://github.com/zyantific/zydis.git --depth 1 -b v4.1.1
IFS=' '
for f in $(echo zydis/tests/cases/*.in); do
	line=$(cat $f)
	arch=$(echo $line | awk -F' ' '{ print $1; }')
	arch=${arch:1:2}
	bytes=$(echo $line | awk -F' ' '{ print $2; }')
	run_test_set "$arch" "$bytes"
done
cd "$old_cwd"
rm -rf "$temp_dir"

echo "Completed $tests_completed tests"
exit $fails
