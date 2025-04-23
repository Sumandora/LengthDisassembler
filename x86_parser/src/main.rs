use std::{fs::OpenOptions, hash::Hash, io::Write};

use itertools::Itertools;
use json::JsonValue;

#[derive(Clone, Debug)]
struct ParsedInstruction {
    pattern: String,
    iclass: String,
    modrm: bool,
    // immediate: u8,
    // fixed_disp: u8,
    fixed: u8, // Under the assumption both are equal

    disp_asz: bool,
    disp_osz: bool,
    imm_osz: bool,  // 64 osz == 32 bits
    uimm_osz: bool, // 64 osz == 64 bits
    cloned_from: Option<u8>,
}

impl PartialEq for ParsedInstruction {
    fn eq(&self, other: &ParsedInstruction) -> bool {
        self.modrm == other.modrm
            // && self.immediate == other.immediate
            // && self.fixed_disp == other.fixed_disp
            && self.fixed == other.fixed
            && self.disp_asz == other.disp_asz
            && self.disp_osz == other.disp_osz
            && self.imm_osz == other.imm_osz
            && self.uimm_osz == other.uimm_osz
    }
}

impl Hash for ParsedInstruction {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        (
            self.modrm,
            // self.immediate,
            // self.fixed_disp,
            self.fixed,
            self.disp_asz,
            self.disp_osz,
            self.imm_osz,
            self.uimm_osz,
        )
            .hash(state)
    }
}

impl Eq for ParsedInstruction {}

fn get_dominant(vec: &[ParsedInstruction]) -> (ParsedInstruction, bool) {
    let map = vec
        .iter()
        .filter(|insn| insn.cloned_from.is_none())
        .cloned()
        .counts();

    if map.is_empty() {
        return (
            vec.iter()
                .max_by_key(|e| e.cloned_from.unwrap())
                .unwrap()
                .clone(),
            false,
        );
    }

    let conflicts = map.len() > 1;

    (
        map.into_iter().max_by_key(|(_, i)| *i).unwrap().0,
        conflicts,
    )
}

fn parse_instructions(instructions: &JsonValue) -> Vec<Vec<Vec<ParsedInstruction>>> {
    let map_count = instructions
        .members()
        .map(|k| k["map"].as_u64().unwrap())
        .max()
        .unwrap();

    let mut parsed_instructions: Vec<Vec<Vec<ParsedInstruction>>> =
        vec![Default::default(); map_count as usize + 1 /* the 0th map -_- */];

    for k in instructions.members() {
        let map = k["map"].as_u64().unwrap() as usize;
        let opcode = u8::from_str_radix(k["opcode_hex"].as_str().unwrap(), 16).unwrap();
        let pattern = k["pattern"].as_str().unwrap();
        let parts = pattern.split(" ").map(|s| s.to_owned()).collect::<Vec<_>>();

        const EXPLICITLY_HANDLED: &[(usize, u8)] = &[
            (0, 0xF7),
            (0, 0xF6),
            (0, 0xA1),
            (0, 0xE8),
            (0, 0xE9),
            // mov cr/dr
            (1, 0x20),
            (1, 0x21),
            (1, 0x22),
            (1, 0x23),
        ];

        if EXPLICITLY_HANDLED.contains(&(map, opcode)) {
            // These ones are handled explicitly in the length disassembler,
            // because they encode too many different instructions...
            continue;
        }

        let has_part = |part| parts.iter().any(|s| s == part);

        let mut fixed_disp = 0;
        if has_part("BRDISP8()") {
            fixed_disp = 1;
        }
        if has_part("BRDISP32()") {
            fixed_disp = 4;
        }
        if has_part("BRDISP64()") {
            fixed_disp = 8;
        }

        if has_part("MEMDISP32()") {
            fixed_disp = 4;
        }
        if has_part("MEMDISP16()") {
            fixed_disp = 2;
        }
        if has_part("MEMDISP8()") {
            fixed_disp = 1;
        }
        if has_part("MEMDISP()") {
            fixed_disp = 4;
        }

        let mut imm_osz = false;
        let mut uimm_osz = false;
        let mut disp_asz = false;
        let mut disp_osz = false;

        if has_part("MEMDISPv()") {
            disp_asz = true;
        }
        if has_part("UIMMv()") {
            uimm_osz = true;
        }
        if has_part("SIMMz()") {
            imm_osz = true;
        }
        if has_part("BRDISPz()") {
            disp_osz = true;
        }

        let modrm = k["has_modrm"].as_bool().unwrap();
        let mut imm = 0;

        if k["has_imm16"].as_bool().unwrap() {
            imm += 2;
        }
        if k["has_imm32"].as_bool().unwrap() {
            imm += 4;
        }
        if k["has_imm8"].as_bool().unwrap() {
            imm += 1;
        }
        if k["has_imm8_2"].as_bool().unwrap() {
            imm += 1;
        }

        if has_part("SE_IMM8()") {
            imm += 1;
        }

        if map == 0 && (0xd0..=0xd3).contains(&opcode) {
            // This pattern features ONE() but doesn't actually use it?
            imm = 0;
        }

        if map == 1 && (0x80..=0x8f).contains(&opcode) && !has_part("MODE!=2") {
            // Jumps with conditions
            // in 16-32 bit they have 2/4 byte offsets,
            // but in 64-bit they are forced to 4
            fixed_disp = 0;
            disp_osz = true;
        }

        assert!(imm == 0 || fixed_disp == 0); // This is used as a compression technique.

        // These 2 conditions can also be used, but currently the struct is exactly 1 byte big,
        // so there is no reason to improve anything until a new field needs to be added.
        assert!(!disp_osz || !disp_asz);
        assert!(!imm_osz || !uimm_osz);

        let insn = ParsedInstruction {
            pattern: k["pattern"].as_str().unwrap().to_owned(),
            iclass: k["iclass"].as_str().unwrap().to_owned(),
            modrm,
            // immediate: imm,
            // fixed_disp,
            fixed: imm | fixed_disp,
            disp_asz,
            disp_osz,
            imm_osz,
            uimm_osz,
            cloned_from: None,
        };

        let opcodes = parsed_instructions.get_mut(map).unwrap();
        if opcodes.is_empty() {
            *opcodes = vec![Default::default(); 256];
        }

        if k["partial_opcode"].as_bool().unwrap() {
            let mut mask = k["opcode"].as_str().unwrap().to_owned();

            assert!(mask.starts_with("0b"));

            mask = mask[2..].to_owned();
            mask = mask.split_once("_").unwrap().0.to_owned();

            assert!(mask.len() == 4);

            let mut num = u8::from_str_radix(&mask, 2).unwrap();
            num <<= 4;
            for x in 0..=0b1111 {
                let new_opcode = num + x;
                if new_opcode < opcode {
                    continue;
                }
                let instructions = opcodes.get_mut(new_opcode as usize).unwrap();

                instructions.push(ParsedInstruction {
                    cloned_from: Some(opcode),
                    ..insn.clone()
                });
            }
        } else {
            let instructions = opcodes.get_mut(opcode as usize).unwrap();

            instructions.push(insn);
        }
    }

    parsed_instructions
}

fn build_fat_table(
    parsed_instructions: &[Vec<Vec<ParsedInstruction>>],
) -> Vec<Vec<Option<ParsedInstruction>>> {
    let mut longest_fixed = 0u8;

    let mut fat_table = OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(true)
        .open("generated_table.h")
        .unwrap();

    writeln!(
        fat_table,
        "// This file has been generated, do not edit manually.\n"
    )
    .unwrap();

    let mut dominated_opcode_map = Vec::<Vec<Option<ParsedInstruction>>>::new();

    for (map, insns) in parsed_instructions.iter().enumerate() {
        let mut dominating_opcodes = Vec::new();
        writeln!(fat_table, "const OPCODE_INFO OPCODE_TABLE_{map}[] = {{").unwrap();
        for opcode in 0x00..=0xFF {
            let empty = Vec::new();
            let instructions = insns.get(opcode).unwrap_or(&empty);
            if instructions.is_empty() {
                writeln!(fat_table, "\tOPCODE_EMPTY_DEF, // {opcode:#x}").unwrap();
                dominating_opcodes.push(None);
                continue;
            }

            let names = instructions.iter().map(|s| s.iclass.clone()).join(", ");
            let (dominant, conflicting) = get_dominant(instructions);

            longest_fixed = longest_fixed.max(dominant.fixed);

            writeln!(
                fat_table,
                "\tOPCODE_INSN_DEF({}, {}, {}, {}, {}, {}), // {} => {:#x}{}{}",
                dominant.modrm,
                dominant.fixed,
                dominant.disp_asz,
                dominant.disp_osz,
                dominant.imm_osz,
                dominant.uimm_osz,
                names,
                opcode,
                if let Some(cloned_from) = dominant.cloned_from {
                    format!(" (Cloned from {cloned_from:#x})")
                } else {
                    "".to_owned()
                },
                if conflicting { " (HAS CONFLICTS)" } else { "" }
            )
            .unwrap();

            dominating_opcodes.push(Some(dominant));

            if conflicting {
                eprintln!("Opcode {opcode:#x} (Map: {map}) has conflicts:");

                for i in instructions {
                    eprintln!("{} => {}", i.iclass, i.pattern);
                }
            }
        }

        writeln!(fat_table, "}};\n").unwrap();

        dominated_opcode_map.push(dominating_opcodes);
    }

    writeln!(fat_table, "const OPCODE_INFO* const OPCODE_TABLES[] = {{").unwrap();
    for (map, _) in parsed_instructions.iter().enumerate() {
        writeln!(fat_table, "\tOPCODE_TABLE_{map},").unwrap();
    }
    writeln!(fat_table, "}};").unwrap();

    eprintln!("Longest fixed: {longest_fixed}");

    dominated_opcode_map
}

fn build_thin_table(table: &[Vec<Option<ParsedInstruction>>]) {
    let mut thin_table = OpenOptions::new()
        .write(true)
        .create(true)
        .truncate(true)
        .open("generated_thin_table.h")
        .unwrap();

    writeln!(
        thin_table,
        "// This file has been generated, do not edit manually.\n"
    )
    .unwrap();

    let mut rules_counts = Vec::new();

    for (map, opcodes) in table.iter().enumerate() {
        eprintln!("Map: {map}");
        writeln!(
            thin_table,
            "const OPCODE_INFO_RANGE OPCODE_TABLE_{map}[] = {{"
        )
        .unwrap();

        let orig_opcodes = opcodes.clone();
        let mut opcodes = opcodes.clone();
        let mut rules = Vec::new();

        loop {
            // This thing down here is far from optimal, it's pretty slow actually, but my Intel Pentium can run it
            // and that means I won't spend more time on optimizing this, perhaps two pointer would help here though.
            let optimization = (0x00..=0xFF)
                .combinations_with_replacement(2)
                .map(|vec| {
                    let a = *vec.first().unwrap() as usize;
                    let b = *vec.get(1).unwrap() as usize;
                    (usize::min(a, b), usize::max(a, b))
                })
                .filter(|(from, to)| {
                    opcodes.get(*from).unwrap().is_some() && opcodes.get(*to).unwrap().is_some()
                })
                .filter(|(from, to)| {
                    (*from..=*to)
                        .map(|i| opcodes.get(i).unwrap())
                        .filter_map(|optional_insn| optional_insn.clone())
                        .dedup()
                        .count()
                        == 1
                })
                .sorted_by_key(|(from, to)| {
                    let count = (*from..=*to)
                        .map(|i| opcodes.get(i).unwrap())
                        .filter_map(|insn| insn.clone())
                        .count() as isize;
                    -count
                })
                .next();
            if let Some((from, to)) = optimization {
                let insn = opcodes.get(from).and_then(|opt| opt.clone()).unwrap();
                eprintln!("Found optimization: {}-{} is {:?}", from, to, insn);
                rules.push(((from, to), insn));
                opcodes
                    .iter_mut()
                    .take(to + 1)
                    .skip(from)
                    .for_each(|opcode| *opcode = None);
                continue;
            }
            assert!(opcodes.iter().any(|e| e.is_none()));
            break;
        }

        for i in 0x00..=0xFF {
            let old = orig_opcodes.get(i).unwrap();
            if old.is_some() {
                assert_eq!(
                    *old.as_ref().unwrap(),
                    rules
                        .iter()
                        .find(|((from, to), _)| i >= *from && i <= *to)
                        .unwrap()
                        .1
                );
            }
        }

        for (range, instruction) in &rules {
            writeln!(
                thin_table,
                "\tRANGE_OPCODE_INSN_DEF({}, {}, OPCODE_INSN_DEF({}, {}, {}, {}, {}, {})),",
                range.0,
                range.1,
                instruction.modrm,
                instruction.fixed,
                instruction.disp_asz,
                instruction.disp_osz,
                instruction.imm_osz,
                instruction.uimm_osz,
            )
            .unwrap();
        }

        writeln!(thin_table, "}};\n").unwrap();
        rules_counts.push(rules.len());
    }

    writeln!(
        thin_table,
        "const OPCODE_TABLE_DEFINITION OPCODE_TABLES[] = {{"
    )
    .unwrap();
    for (map, _) in table.iter().enumerate() {
        writeln!(
            thin_table,
            "\tOPCODE_TABLE_DEF(OPCODE_TABLE_{map}, {}),",
            rules_counts.get(map).unwrap()
        )
        .unwrap();
    }
    writeln!(thin_table, "}};").unwrap();
}

fn main() {
    let json = std::fs::read_to_string("./test.json").unwrap();
    let parsed = json::parse(&json).unwrap();

    let instructions = &parsed["Instructions"];

    let parsed_instructions = parse_instructions(instructions);
    let dominating_opcode_map = build_fat_table(&parsed_instructions);
    build_thin_table(&dominating_opcode_map);
}
