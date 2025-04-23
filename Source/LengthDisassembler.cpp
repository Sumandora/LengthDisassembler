#include "LengthDisassembler/LengthDisassembler.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <utility>

#include "ByteStream.hpp"
#include "Opcodes.hpp"

// NOTE: If you plan on reading through this entire code, please start at the LengthDisassembler::disassemble function

using namespace LengthDisassembler;

// NOLINTBEGIN(cppcoreguidelines-macro-usage, bugprone-macro-parentheses, cppcoreguidelines-avoid-do-while)
#define PROPAGATE_RESULT_AND_DEFINE(name, result)         \
	decltype((result))::value_type name;                  \
	do {                                                  \
		if (auto _result = (result); _result.has_value()) \
			name = _result.value();                       \
		else                                              \
			return std::unexpected(_result.error());      \
	} while (0)
#define PROPAGATE_RESULT(result)                           \
	do {                                                   \
		if (auto _result = (result); !_result.has_value()) \
			return std::unexpected(_result.error());       \
	} while (0)
#define NO_MORE_DATA_IF(result)                          \
	do {                                                 \
		if ((result))                                    \
			return std::unexpected(Error::NO_MORE_DATA); \
	} while (0)
// NOLINTEND(cppcoreguidelines-macro-usage, bugprone-macro-parentheses, cppcoreguidelines-avoid-do-while)

static bool parse_rex_prefix(std::uint8_t byte, bool& operand_size_override)
{
	// Check that bits follow the pattern 0b0100xxxx
	const bool has_rex = (byte & 0b11110000) == 0b01000000;

	if (has_rex)
		operand_size_override = (byte >> 3) & 0b1; // REX.W

	return has_rex;
};

static std::uint8_t legacy_prefixes[]{
	0xF0,
	0xF2,
	0xF3,

	0x2E,
	0x36,
	0x3E,
	0x26,
	0x64,
	0x65,
	0x2E,
	0x3E,

	0x66,

	0x67,
};

static void count_prefixes(ByteStream& bytes,
	bool& operand_override_prefix,
	bool& address_override_prefix,
	bool& operand_size_override,
	bool search_for_rex_prefix)
{
	while (!bytes.empty()) {
		const std::uint8_t next = bytes.peek().value();
		if (std::uint8_t* it = std::ranges::find(legacy_prefixes, next);
			it != std::end(legacy_prefixes)) {
			if (*it == 0x66)
				operand_override_prefix = true;
			if (*it == 0x67)
				address_override_prefix = true;

			// This is undefined/undocumented. When there are multiple REX prefixes, the last one counts, but
			// if there is another legacy prefix after the REX prefix, then the REX prefix becomes invalid/is forgotten about.
			operand_size_override = false;

			bytes.next();
			continue;
		}

		if (search_for_rex_prefix && parse_rex_prefix(next, operand_size_override)) {
			bytes.next();
			continue;
		}

		break;
	}
}

static std::expected<void, Error> parse_opcode(ByteStream& bytes, std::uint8_t& opcode, std::uint8_t& opcode_map)
{
	auto first = bytes.next();
	if (first.has_value() && first != 0x0F) {
		opcode = first.value();
		opcode_map = 0;
		return {};
	}

	auto second = bytes.next();
	if (second.has_value() && second != 0x38 && second != 0x3A) {
		opcode = second.value();
		opcode_map = 1;
		return {};
	}

	auto third = bytes.next();
	NO_MORE_DATA_IF(!third);

	opcode = third.value();
	opcode_map = second == 0x38 ? 2 : 3;
	return {};
}

struct SIB {
	std::uint8_t scale : 2;
	std::uint8_t index : 3;
	std::uint8_t base : 3;

	std::expected<void, Error> parse(ByteStream& bytes)
	{
		const std::optional<std::uint8_t> optional = bytes.next();
		NO_MORE_DATA_IF(!optional);
		const std::uint8_t byte = optional.value();

		*this = {
			.scale = static_cast<std::uint8_t>((byte >> 6) & 0b11),
			.index = static_cast<std::uint8_t>((byte >> 3) & 0b111),
			.base = static_cast<std::uint8_t>((byte >> 0) & 0b111)
		};

		return {};
	}
};

struct ModRM {
	std::uint8_t mod : 2;
	std::uint8_t reg : 3;
	std::uint8_t rm : 3;

	std::expected<void, Error> parse(ByteStream& bytes,
		std::uint8_t& displacement,
		bool addressing_with_16bit)
	{
		const std::optional<std::uint8_t> optional = bytes.next();
		NO_MORE_DATA_IF(!optional);
		const std::uint8_t byte = optional.value();

		*this = {
			.mod = static_cast<std::uint8_t>((byte >> 6) & 0b11),
			.reg = static_cast<std::uint8_t>((byte >> 3) & 0b111),
			.rm = static_cast<std::uint8_t>((byte >> 0) & 0b111),
		};

		if (addressing_with_16bit) {
			switch (mod) {
			case 0b00:
				if (rm == 0b110)
					displacement = 4;
				break;
			case 0b01:
				displacement = 1;
				break;
			case 0b10:
				displacement = 2;
				break;
			default:
				break;
			}

			return {};
		}

		SIB sib{};

		if (mod != 0b11 && rm == 0b100) {
			PROPAGATE_RESULT(sib.parse(bytes));
		}

		switch (mod) {
		case 0b00:
			if (rm == 0b101)
				// TODO alert user that there is a relative displacement (if on 64 bit)
				displacement = 4;

			if (sib.base == 0b101)
				displacement = 4;
			break;
		case 0b01:
			displacement = 1;
			break;
		case 0b10:
			displacement = 4;
			break;
		default:
			break;
		}

		return {};
	}
};

enum class VexType : std::uint8_t {
	TWO_BYTE,
	THREE_BYTE,
	// This thing must be the greatest invention AMD ever brought forward.
	THREE_BYTE_XOP,
	EVEX,
};

static std::optional<VexType> type_of_vex(MachineMode mode, const ByteStream& bytes)
{
	if (!bytes.has(2)) {
		// Even the shortest vex (two-byte vex) is 2 bytes long.
		return std::nullopt;
	}

	using enum VexType;

	if (mode == MachineMode::LONG_COMPATIBILITY_MODE) {
		// Some opcodes may clash with VEX, for example 0x62 is also the opcode for BOUND
		// To disambiguate Intel suggests checking the bits of the next byte
		// The VEX.R (first bit) is useless as only 8 registers are available.
		// Thus VEX.R is used to check if the byte is a EVEX or a BOUND in this case
		const std::uint8_t vex1 = bytes.peek(1).value();
		const std::uint8_t vex_r = (vex1 >> 7) & 0b1;
		// Just checking VEX.R does not work, X also needs to be checked, not sure if this is enough.
		const std::uint8_t vex_x = (vex1 >> 6) & 0b1;

		if (vex_r == 0 || vex_x == 0)
			// TODO is this correct?
			return std::nullopt;
	}

	const std::uint8_t vex_byte = bytes.peek().value();
	switch (vex_byte) {
	case 0xC4:
		if (bytes.has(3))
			return THREE_BYTE;

		break;
	case 0xC5:
		// The top of the function already verifies at least 2 bytes
		return TWO_BYTE;
	case 0x8F: {
		const std::uint8_t opcode_map = bytes.peek(1).value() & 0b11111;
		if (opcode_map >= 8) // Prevent it from overlapping with other instructions...
			return THREE_BYTE_XOP;

		break;
	}
	case 0x62:
		if (bytes.has(4))
			return EVEX;

		break;
	default:;
	};
	return std::nullopt;
}

static std::uint8_t parse_two_byte_vex(ByteStream& bytes, std::uint8_t& opcode_map)
{
	assert(bytes.has(2));

	assert(bytes.next().value() == 0xC5);

	opcode_map = 0b00001;

	(void)bytes.next();
	return 2;
}

static std::uint8_t parse_three_byte_vex(ByteStream& bytes,
	std::uint8_t& opcode_map,
	bool& operand_size_override)
{
	assert(bytes.has(3));

	assert(bytes.next().value() == 0xC4);

	opcode_map = bytes.next().value() & 0b11111;

	operand_size_override = (bytes.next().value() >> 7) & 0b1; // VEX.W

	return 3;
}

static std::uint8_t parse_three_byte_xop(ByteStream& bytes,
	std::uint8_t& opcode_map,
	bool& operand_size_override)
{
	assert(bytes.has(3));

	assert(bytes.next().value() == 0x8F);

	opcode_map = bytes.next().value() & 0b11111;

	// The AMD64 Architecture Programmer’s Manual Volume 6 states that the map_select field must be equal to or greater than 8, to differentiate the XOP prefix from the POP instruction that formerly used opcode 0x8F.
	assert(opcode_map >= 8);

	operand_size_override = (bytes.next().value() >> 7) & 0b1; // VEX.W

	return 3;
}

static std::uint8_t parse_evex(ByteStream& bytes,
	std::uint8_t& opcode_map,
	bool& operand_size_override)
{
	assert(bytes.has(4));

	assert(bytes.next().value() == 0x62);
	opcode_map = bytes.next().value() & 0b111;

	operand_size_override = (bytes.next().value() >> 7) & 0b1; // VEX.W

	(void)bytes.next();

	return 4;
}

static bool is_3dnow(const ByteStream& bytes)
{
	return bytes.has(2) && bytes.peek() == 0x0F && bytes.peek(1) == 0x0F;
}

static std::expected<void, Error> handle_3dnow(ByteStream& bytes, bool addressing_with_16bit, std::uint8_t& map, std::uint8_t& opcode)
{
	assert(bytes.consume(2)); // 0x0F0F

	std::uint8_t displacement = 0;
	PROPAGATE_RESULT(ModRM{}.parse(bytes, displacement, addressing_with_16bit));
	NO_MORE_DATA_IF(!bytes.consume(displacement));

	static constexpr std::uint8_t OPCODE_MAP_3D_NOW = 4; // All 3DNOW instructions reside in map 4
	map = OPCODE_MAP_3D_NOW;

	if (auto opcode_byte = bytes.next(); opcode_byte.has_value()) {
		opcode = opcode_byte.value();

		return {};
	}

	return std::unexpected(Error::NO_MORE_DATA);
}

/*
 * Address and Operands size overrides in Long 64-bit mode:
 *      REX.W   Prefix  Operand     Address
 *      0       No      32-bit      64-bit
 *      0       Yes     16-bit      32-bit
 *      1       No      64-bit[1]   64-bit
 *      1       Yes     64-bit      32-bit
 *
 * [1] Some instructions don't need REX.W for 64-bit operands
 *
 *
 * Long compatibility mode:
 *
 *      Prefix  Operand     Address
 *      No      32-bit      32-bit
 *      Yes     16-bit      16-bit
 */

// TODO, when VEX implies 0x66 prefix, does that count?

static std::uint8_t get_address_size(MachineMode mode, bool prefix)
{
	switch (mode) {
	case MachineMode::VIRTUAL8086:
		return prefix ? 32 : 16;
	case MachineMode::LONG_COMPATIBILITY_MODE:
		return prefix ? 16 : 32;
	case MachineMode::LONG_MODE:
		return prefix ? 32 : 64;
	default:
		std::unreachable();
	}
}

static std::uint8_t get_operand_size(MachineMode mode, bool rex_w, bool prefix)
{
	switch (mode) {
	case MachineMode::VIRTUAL8086:
		return prefix ? 32 : 16;
	case MachineMode::LONG_COMPATIBILITY_MODE:
		return prefix ? 16 : 32;
	case MachineMode::LONG_MODE:
		if (!rex_w)
			return prefix ? 16 : 32;
		return 64;
	default:
		std::unreachable();
	}
}

static std::expected<bool, Error> handle_instructions_explicitly(ByteStream& stream, Instruction& instruction, MachineMode mode)
{
	const bool addressing_with_16bit = instruction.address_bits == 16;

	if (instruction.opcode == 0xf7 && instruction.opcode_map == 0) {
		std::uint8_t displacement = 0;
		ModRM modrm{};
		PROPAGATE_RESULT(modrm.parse(stream, displacement, addressing_with_16bit));
		NO_MORE_DATA_IF(!stream.consume(displacement));
		if (modrm.reg == 0b0000 || modrm.reg == 0b0001) {
			const std::uint8_t bytes = std::min(instruction.operand_bits / 8, 4);
			NO_MORE_DATA_IF(!stream.consume(bytes));
		}
		return true;
	}
	if (instruction.opcode == 0xf6 && instruction.opcode_map == 0) {
		std::uint8_t displacement = 0;
		ModRM modrm{};
		PROPAGATE_RESULT(modrm.parse(stream, displacement, addressing_with_16bit));
		NO_MORE_DATA_IF(!stream.consume(displacement));
		if (modrm.reg == 0b0000 || modrm.reg == 0b0001) {
			NO_MORE_DATA_IF(!stream.next());
		}
		return true;
	}
	if (instruction.opcode == 0xa1 && instruction.opcode_map == 0) {
		// This instruction purposely ignores prefixes...
		switch (mode) {
		case MachineMode::VIRTUAL8086:
			NO_MORE_DATA_IF(!stream.consume(2));
			break;
		case MachineMode::LONG_COMPATIBILITY_MODE:
			NO_MORE_DATA_IF(!stream.consume(4));
			break;
		case MachineMode::LONG_MODE:
			NO_MORE_DATA_IF(!stream.consume(8));
			break;
		default:
			std::unreachable();
		}

		return true;
	}
	if (instruction.opcode == 0x78 && instruction.opcode_map == 1) {
		if (!instruction.is_vex) {
			// VMREAD or EXTRQ or INSERTQ
			// TODO check that its not VMREAD
			std::uint8_t displacement = 0;
			ModRM modrm{};
			PROPAGATE_RESULT(modrm.parse(stream, displacement, addressing_with_16bit));
			NO_MORE_DATA_IF(!stream.consume(displacement));
			NO_MORE_DATA_IF(!stream.consume(2)); // two 1-byte immediate
			return true;
		}
	}
	if (instruction.opcode_map == 0 && (instruction.opcode == 0xe8 || instruction.opcode == 0xe9)) {
		switch (mode) {
		case MachineMode::VIRTUAL8086:
			NO_MORE_DATA_IF(!stream.consume(2));
			break;
		case MachineMode::LONG_COMPATIBILITY_MODE:
			NO_MORE_DATA_IF(!stream.consume(instruction.operand_bits / 8));
			break;
		case MachineMode::LONG_MODE:
			// TODO: Alert user that there is a relative offset
			NO_MORE_DATA_IF(!stream.consume(4));
			break;
		default:
			std::unreachable();
		}
		return true;
	}
	if (instruction.opcode_map == 1 && (instruction.opcode == 0x20 || instruction.opcode == 0x21)) {
		// MOV CR/DR, take modrm, but just don't care about its displacement...
		NO_MORE_DATA_IF(!stream.next());
		return true;
	}

	return false;
}

std::expected<Instruction, Error> LengthDisassembler::disassemble(std::byte* bytes, MachineMode mode, std::uint8_t max_length)
{
	ByteStream stream{ bytes, static_cast<std::uint8_t>(max_length + 1) };

	Instruction instruction{
		.length = 0,

		.opcode_map = 0,
		.opcode = 0,

		.address_bits = 0,
		.operand_bits = 0,

		.operand_override_prefix = false,
		.address_override_prefix = false,

		.operand_size_override = false,

		.is_vex = false,
		.is_3dnow = false,
	};

	count_prefixes(stream,
		instruction.operand_override_prefix,
		instruction.address_override_prefix,
		instruction.operand_size_override,
		mode == MachineMode::LONG_MODE);

	NO_MORE_DATA_IF(stream.empty());

	if (const std::optional<VexType> type = type_of_vex(mode, stream); type.has_value()) {
		instruction.is_vex = true;
		switch (type.value()) {
			using enum VexType;
		case TWO_BYTE:
			parse_two_byte_vex(stream, instruction.opcode_map);
			break;
		case THREE_BYTE:
			parse_three_byte_vex(stream, instruction.opcode_map, instruction.operand_size_override);
			break;
		case THREE_BYTE_XOP:
			parse_three_byte_xop(stream, instruction.opcode_map, instruction.operand_size_override);
			break;
		case EVEX:
			parse_evex(stream, instruction.opcode_map, instruction.operand_size_override);
			break;
		default:
			std::unreachable();
		}
		if (std::optional<std::uint8_t> optional = stream.next(); optional.has_value()) {
			instruction.opcode = optional.value();
		} else {
			return std::unexpected(Error::NO_MORE_DATA);
		}
	}

	instruction.address_bits = get_address_size(mode, instruction.address_override_prefix);
	instruction.operand_bits = get_operand_size(mode,
		instruction.operand_size_override,
		instruction.operand_override_prefix);

	const bool addressing_with_16bit = instruction.address_bits == 16;

	if (!instruction.is_vex) {
		if (is_3dnow(stream)) {
			instruction.is_3dnow = true;
			PROPAGATE_RESULT(handle_3dnow(stream, addressing_with_16bit, instruction.opcode_map, instruction.opcode));
			instruction.length = stream.offset();
			return instruction;
		}

		PROPAGATE_RESULT(parse_opcode(stream, instruction.opcode, instruction.opcode_map));
	}

	PROPAGATE_RESULT_AND_DEFINE(explicitly_handled, handle_instructions_explicitly(stream, instruction, mode));
	if (explicitly_handled) {
		instruction.length = stream.offset();
		return instruction;
	}

	const Opcodes::OpcodeInfo* info = Opcodes::lookup(instruction.opcode_map, instruction.opcode);

	if (!info) {
		return std::unexpected(Error::UNKNOWN_INSTRUCTION);
	}

	std::uint8_t displacement = 0;
	if (info->modrm) {
		PROPAGATE_RESULT(ModRM{}.parse(stream, displacement, addressing_with_16bit));
	}

	if (info->disp_asz) {
		NO_MORE_DATA_IF(!stream.consume(instruction.address_bits / 8));
	}

	if (info->disp_osz) {
		const std::uint8_t bytes = std::min(instruction.operand_bits / 8, 4);
		NO_MORE_DATA_IF(!stream.consume(bytes));
	}

	NO_MORE_DATA_IF(!stream.consume(displacement));

	NO_MORE_DATA_IF(!stream.consume(info->fixed));

	if (info->imm_osz) {
		const std::uint8_t bytes = std::min(instruction.operand_bits / 8, 4);
		NO_MORE_DATA_IF(!stream.consume(bytes));
	}

	if (info->uimm_osz) {
		const std::uint8_t bytes = instruction.operand_bits / 8;
		NO_MORE_DATA_IF(!stream.consume(bytes));
	}

	instruction.length = stream.offset();
	return instruction;
}
