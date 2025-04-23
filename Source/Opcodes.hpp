#ifndef OPCODES_HPP
#define OPCODES_HPP

#include <cassert>
#include <cstdint>
#include <iterator>

namespace Opcodes {

	struct [[gnu::packed]] OpcodeInfo {
		bool modrm : 1;

		std::uint8_t fixed : 3;

		bool disp_asz : 1;
		bool disp_osz : 1;
		bool imm_osz : 1;
		bool uimm_osz : 1;
	};

	struct [[gnu::packed]] OpcodeInfoRange {
		std::uint8_t from;
		std::uint8_t to;
		OpcodeInfo info;
	};

	struct [[gnu::packed]] OpcodeTableDefinition {
		const OpcodeInfoRange* ranges;
		std::uint8_t len;
	};

	// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define OPCODE_INFO_RANGE OpcodeInfoRange
#define OPCODE_TABLE_DEFINITION OpcodeTableDefinition
#define OPCODE_INSN_DEF(modrm, fixed, disp_asz, disp_osz, imm_osz, uimm_osz) \
	{ modrm, fixed, disp_asz, disp_osz, imm_osz, uimm_osz }
#define RANGE_OPCODE_INSN_DEF(from, to, info) { from, to, info }
#define OPCODE_TABLE_DEF(table_name, length) { table_name, length }
	// NOLINTEND(cppcoreguidelines-macro-usage)

#include "GeneratedOpcodeTables.h"

#undef OPCODE_TABLE_DEF
#undef RANGE_OPCODE_INSN_DEF
#undef OPCODE_INSN_DEF
#undef OPCODE_TABLE_DEFINITION 
#undef OPCODE_INFO_RANGE 

	constexpr const OpcodeInfo* lookup(std::uint8_t map, std::uint8_t opcode)
	{
		if (map >= std::size(OPCODE_TABLES))
			return nullptr;

		const OpcodeTableDefinition& table = OPCODE_TABLES[map];
		const OpcodeInfoRange* ranges = table.ranges;

		for (std::uint8_t i = 0; i < table.len; i++)
			if (opcode >= ranges[i].from && opcode <= ranges[i].to)
				return &ranges[i].info;

		return nullptr;
	}
}

#endif
