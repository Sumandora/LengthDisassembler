#include "LengthDisassembler/LengthDisassembler.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <iostream>
#include <numeric>
#include <print>
#include <string>
#include <utility>
#include <vector>

#include <Zycore/Status.h>
#include <Zydis/Disassembler.h>
#include <Zydis/SharedTypes.h>

using namespace LengthDisassembler;

int main(int argc, const char** argv)
{
	assert(argc == 2);

	using enum LengthDisassembler::MachineMode;

	MachineMode mode = LONG_MODE;
	if (strcmp(argv[1], "16") == 0) {
		mode = VIRTUAL8086;
	} else if (strcmp(argv[1], "32") == 0) {
		mode = LONG_COMPATIBILITY_MODE;
	} else if (strcmp(argv[1], "64") == 0) {
		mode = LONG_MODE;
	} else {
		std::println(std::cerr, "Expected 16/32/64 bit as argv[1], got '{}'", argv[1]);
		return 1;
	}

	int failed_tests = 0;

	for (std::string hex_string; std::getline(std::cin, hex_string);) {
		std::vector<std::byte> nibbles;

		for (std::size_t i = 0; i < hex_string.length(); i += 2) {
			const std::string hex_number{ hex_string.substr(i, 2) };
			int b = std::stoi(hex_number, nullptr, 16);
			nibbles.push_back(static_cast<std::byte>(b));
		}

		ZydisMachineMode zydis_mode = ZYDIS_MACHINE_MODE_LONG_64;
		switch (mode) {
		case MachineMode::VIRTUAL8086:
			zydis_mode = ZYDIS_MACHINE_MODE_REAL_16;
			break;
		case MachineMode::LONG_COMPATIBILITY_MODE:
			zydis_mode = ZYDIS_MACHINE_MODE_LONG_COMPAT_32;
			break;
		case MachineMode::LONG_MODE:
			zydis_mode = ZYDIS_MACHINE_MODE_LONG_64;
			break;
		}

		ZydisDisassembledInstruction instruction;
		if (!ZYAN_SUCCESS(ZydisDisassembleIntel(zydis_mode,
				0,
				nibbles.data(),
				nibbles.size(),
				&instruction))) {
			continue;
		}

		std::expected<Instruction, Error> result = disassemble(nibbles.data(), mode);

		if(!result.has_value()) {
			std::println(std::cerr, "Disassembly of '{}' failed with error: {}", hex_string, std::to_underlying(result.error()));
			failed_tests = std::add_sat(failed_tests, 1);
			continue;
		}

		std::uint8_t guess = result.value().length;

		if (guess != instruction.info.length) {
			std::println(std::cerr, "Expected {} but got {} on {}", instruction.info.length, guess, hex_string);
			failed_tests = std::add_sat(failed_tests, 1);
		}
	}

	return failed_tests;
}
