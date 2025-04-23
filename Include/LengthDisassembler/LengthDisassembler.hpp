#ifndef LENGTHDISASSEMBLER_HPP
#define LENGTHDISASSEMBLER_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>

namespace LengthDisassembler {
	enum class MachineMode : std::uint8_t {
		VIRTUAL8086, // 8086, WARNING: This mode is the least supported. The opcodes tables have been generated for the other modes.
		LONG_COMPATIBILITY_MODE, // x86
		LONG_MODE, // x86-64
	};

	struct Instruction {
		std::uint8_t length;

		std::uint8_t opcode_map;
		std::uint8_t opcode;

		std::uint8_t address_bits;
		std::uint8_t operand_bits;

		bool operand_override_prefix;
		bool address_override_prefix;

		bool operand_size_override;

		bool is_vex;
		bool is_3dnow;
	};

	enum class Error : std::uint8_t {
		NO_MORE_DATA, // The byte array ended prematurely, no instruction can be parsed from it.
		UNKNOWN_INSTRUCTION, // The instruction wasn't found in the opcode tables. WARNING: Invalid instructions can slip past this, calling `disassemble` on invalid encodings is undefined behavior, the opcode table is optimized to never expect this kind of error.
	};

	std::expected<Instruction, Error> disassemble(
		std::byte* bytes,
		MachineMode mode = MachineMode::LONG_MODE,
		std::uint8_t max_length = std::numeric_limits<std::uint8_t>::max());
}

#endif
