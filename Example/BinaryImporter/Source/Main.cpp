#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <functional>
#include <iostream>
#include <print>
#include <set>
#include <span>
#include <string>

#include "Zycore/Status.h"
#include "Zydis/DecoderTypes.h"
#include "Zydis/Disassembler.h"
#include "Zydis/SharedTypes.h"

int main(int argc, const char** argv)
{
	assert(argc == 3);

	ZydisMachineMode mode = ZYDIS_MACHINE_MODE_LONG_64;
	if (strcmp(argv[1], "16") == 0) {
		mode = ZYDIS_MACHINE_MODE_REAL_16;
	} else if (strcmp(argv[1], "32") == 0) {
		mode = ZYDIS_MACHINE_MODE_LONG_COMPAT_32;
	} else if (strcmp(argv[1], "64") == 0) {
		mode = ZYDIS_MACHINE_MODE_LONG_64;
	} else {
		std::println(std::cerr, "Expected 16/32/64 bit as argv[1], got '{}'", argv[1]);
		return 1;
	}

	FILE* h = fopen(argv[2], "r");

	if (!h) {
		perror("fopen");
		return 1;
	}

	std::set<std::size_t> hashes;

	std::uint8_t buf[32];
	while (!feof(h)) {
		const std::size_t i = fread(buf, 1, 32, h);
		if (i < 32)
			break;
		if (fseek(h, -32, SEEK_CUR) < 0) {
			perror("fseek");
			return 1;
		}

		ZydisDisassembledInstruction instruction{};
		if (!ZYAN_SUCCESS(ZydisDisassembleIntel(mode,
				0,
				buf,
				sizeof(buf),
				&instruction))) {
			std::println(std::cerr, "Failed to disassemble instruction at {}", ftell(h));
			if (fseek(h, 1, SEEK_CUR) < 0) {
				perror("fseek");
				return 1;
			}
			continue;
		}
		if (fseek(h, instruction.info.length, SEEK_CUR) < 0) {
			perror("fseek");
			return 1;
		}

		for (const auto & op : instruction.operands) {
				switch (op.type) {
			case ZYDIS_OPERAND_TYPE_UNUSED:
			case ZYDIS_OPERAND_TYPE_REGISTER:
				continue;
			case ZYDIS_OPERAND_TYPE_MEMORY:
				if (op.mem.type == ZYDIS_MEMOP_TYPE_MEM || op.mem.type == ZYDIS_MEMOP_TYPE_AGEN) {
					for (int j = op.mem.disp.offset; j < op.mem.disp.offset + op.mem.disp.size / 8; j++) {
						buf[j] = 0x41;
					}
				}
				break;
			case ZYDIS_OPERAND_TYPE_POINTER:
				continue;
			case ZYDIS_OPERAND_TYPE_IMMEDIATE:
				if (op.imm.is_relative) {
					for (int j = op.imm.offset; j < op.imm.offset + op.imm.size / 8; j++) {
						buf[j] = 0x41;
					}
				}

				break;
			}
		}

		std::string nibbles;
		for (std::uint8_t x : std::span<std::uint8_t>(buf, buf + instruction.info.length)) {
			nibbles += std::format("{:02x}", x);
		}

		const std::size_t hash = std::hash<std::string>{}(nibbles);
		if (!hashes.contains(hash)) {
			std::println("{}", nibbles);
			hashes.insert(hash);
		}
	}

	(void)fclose(h);
}
