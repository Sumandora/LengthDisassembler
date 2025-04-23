# Length Disassembler

A [length disassembler](https://en.wikipedia.org/wiki/Disassembler#Length_disassembler) is a kind of disassembler that only cares about getting the length of an instruction right.

## Use Cases

The use for a length disassembler is quite limited, however there are some popular ones like:

- Chunking instructions to then multi-thread the real disassembly
- Figuring out how many instructions need to be stolen when creating a trampoline/detour hook

## Usage

Simple use cases might look like this:

```c++
#include "LengthDisassembler/LengthDisassembler.hpp"

int main() {
  void* symbol = reinterpret_cast<void*>(strcmp);

  auto result = LengthDisassembler::disassemble(reinterpret_cast<std::byte*>(symbol), LengthDisassembler::MachineMode::LONG_MODE);

  if(!result.has_value()) {
    // Handle an invalid instruction
  }

  auto instruction_length = result.value().length;
}
```

> [!CAUTION]  
> An invalid instruction does not require the length disassembler to return an error.
> The opcode tables are optimized in a way that may mislead the disassembler to think that instructions exist, that are actually bogus.
> If you are chunking byte arrays and need correctness, then run another disassembler like [XED from Intel](https://github.com/intelxed/xed) on the results and backtrack when a difference happens.

## Correctness

As mentioned invalid instructions may not be recognized as such, however for valid instructions, there are several test sets checking the most common instructions and a few edge cases.

The test suites currently implemented are:

- Zydis: The Zydis decoder tests are mostly checking for edge cases and don't test a wide variety
- Radare2: The Radare2 tests are a lot of instructions that commonly occur in binaries, so they test for real-world performance

All of these tests are being passed\[1\], coming out at 3741 tests.

The last test suites are imported binaries, currently there is one:

- Rust-analyzer: This is every instruction found in the `.text` section of the rust-analyzer binary provided by the Arch Linux repositories.

More imported tests may be added in the future.

With the imported binaries this comes out at 61421 tests, although many are the same just with different registers. The importer tool only gets rid of changing addresses, not changing registers.

\[1\] Throughout the tests Zydis is used as a baseline, if Zydis is not familiar with an instruction, then it is not tested against.

## Licenses

This entire repository is licensed under the MIT license, except for the test suites imported from other projects. Those have their own licenses, further information can be found at the top of the `./Example/runner.sh` script.
