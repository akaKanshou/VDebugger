# VDebugger

A debugger for x86-64 linux, written entirely in C.


## Features

- Stack unwinding, even with frame pointer omission
- Step-in, Step-out, Step-over
- Set breakpoints on address as well source lines
- Read variables from debugee
- DWARF locexpr evaluator in locexpr.c


## Usage

```bash
vdb ./example.out
vdb> break enable line /path/to/source/file <line>;

OK
vdb> continue;

OK
vdb> var get local struct_var;
var (declared at line 15): {[0]i: 2, [1]c: 98, [2]d: 4.000000}

OK
vdb> continue;

OK
Debugee terminated
```


## Installation

Build the debugger with CMake
```bash
  git clone --recurse-submodules https://github.com/akaKanshou/VDebugger.git
  cd VDebugger/
  cmake -S . -B build/
  cmake --build build/
```
Depends on libdwarf and libunwind to compile. On Arch-based platforms, install using the following commands
```bash
  pacman -S libdwarf libunwind
```
## License

[MIT](https://choosealicense.com/licenses/mit/)

