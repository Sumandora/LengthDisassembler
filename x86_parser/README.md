# x86_parser

This is the tool that generates the opcode tables. If you are here it is likely that you are interested in regenerating the `opcode_infos.h`.

> [!CAUTION]
> The regeneration relies on several out of tree projects, that are not pinned to any version.
> The generated json file is also quite unpredictable as in new features may be added, which break this generator

The commits that where used to generate the current version of the opcode_tables.h file are

| Project   | Commit Hash                              |
| --------- | ---------------------------------------- |
| mbuild    | 7c4497f41f576b43a80bb0f8d8452bbcfd58b6e2 |
| xed       | 1bdc793f5f64cf207f6776f4c0e442e39fa47903 |
| xed_utils | d54a280ea1e1d3ea50ecd865d999b929d4f3c656 |

Here is a tutorial on how to regenerate the tables this:

```bash
$ mkdir xed-build
$ cd xed-build
$ git clone https://github.com/intelxed/xed --depth 1
$ git clone https://github.com/intelxed/mbuild --depth 1
$ git clone https://github.com/ctchou/xed_utils --depth 1
$ mkdir build
$ cd build
$ ../xed/mfile.py just-prep
$ ../xed_utils/xed_db.py -j test.json
```

A `test.json` should have appeared in the current directory (`./xed-build/build`).

This test.json should be placed in the same directory as the x86_parser (`./x86_parser/test.json`).

Then run the parser with `cargo run`, feel free to use `cargo run --release` as it is faster, once compiled.

The generation of the compressed table is quite slow, but even on low-end hardware it only takes a couple of seconds, so I won't optimize it.

## Credits

- [Ching-Tsun Chou](https://github.com/ctchou), for the [xed_utils](https://github.com/ctchou/xed_utils) repository
- [Intel](https://github.com/intelxed), for the [XED](https://github.com/intelxed/xed) decoder
