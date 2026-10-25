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
vdb <path_to_executable> <cmdline_arguments_to_program>
```

## Example

```
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

## Commands

Following is the complete set of commands currently

- Breakpoints

  Set or unset breakpoints.

  ```
  break { enable | disable } { line <source_file_absolute_path> <line_no> | addr <pc_offset> }
  ```

- Continue

  Continue the execution of the debugee upto some breakpoint

  ```
  continue
  ```

- Step

  Step through the program some instructions at a time

  ```
  step <no_of_instructions>
  ```

- Next

  Execute the program till the next line in the current function is reached.

  ```
  next
  ```

- Finish

  Execute the program till the current function finishes execution

  ```
  finish
  ```

- Where

  Get the current source line, address, or function being executed in the debugee.

  ```
  where { line | addr | func }
  ```

- Registers

  Get the value of all registers

  ```
  reg read a
  ```

  Get or set the value of some register

  ```
  reg { read | set } <{ s <register_abbreviation> | d <dwarf_register_num> }> [val]
  ```

  Provide val only if setting register value.

  Register abbreviations (Dwarf numbers)

  - r15 (15)
  - r14 (14)
  - r13 (13)
  - r12 (12)
  - rbp (6)
  - rbx (3)
  - r11 (11)
  - r10 (10)
  - r9 (9)
  - r8 (8)
  - rax (0)
  - rcx (2)
  - rdx (1)
  - rsi (4)
  - rdi (5)
  - orig_rax (x)
  - rip (x)
  - cs (1)
  - eflags (9)
  - rsp (7)
  - ss (2)
  - fs_base (8)
  - gs_base (9)
  - ds (3)
  - es (0)
  - fs (4)
  - gs (5)

- Peek

  Get one word of data from the specified address in the debugee's address space.
  Note: The address needs to be absolute, not an offset to the program's load address.

  ```
  peek <address>
  ```

- Variables

  Get the value or the address of the parameters or the local variables of the current functions

  ```
  var {get | get&} {params | local <var_name>}
  ```

- Stack Trace

  Get the stack trace

  ```
  backtrace
  ```
