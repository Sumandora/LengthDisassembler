# Binary Importer

This is a simple tool that uses Zydis to extract instructions from a binary file.

## Usage

To extract an executable ELF, use this command

```bash
objcopy -O binary --only-section=.text /usr/bin/ls ./ls-text-section
```

Then feed it into the tool like this:

```bash
./BinaryImporter 64 ./ls-text-section
```

The "64" stands for 64 bit, 32 and 16 bit are also accepted, although test sets for 16 bit may not be as relevant.

This tool by itself sends its output to standard output, using a shell, you can redirect it

```bash
./BinaryImporter 64 ./ls-text-section > ls.txt
```

This file can then be moved into the `../TestCases` directory.

To then make the runner.sh script test the new inputs add a line below the currently tested imported instructions. The syntax for these lines should not require further elaboration.
