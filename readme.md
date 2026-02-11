# QBUILD Technical Documentation

## Overview

**QBUILD** is a DOS-based build system designed for QuickBASIC 4.5 projects that include:

* Multiple `.BAS` modules
* External Assembly (`.ASM`) modules
* Quick Libraries (`.QLB`) for IDE integration
* Standalone executable builds (`.EXE`)

QBUILD recreates a historically accurate QuickBASIC 4.5 development workflow while automating:

* Assembly compilation (TASM)
* BASIC compilation (BC)
* Library generation (LINK /Q)
* Executable linking (LINK)
* IDE project generation (.MAK)
* Launch script creation

The system is designed for:

* Real MS-DOS
* DOSBox
* Retro hardware environments (MiSTer, etc.)

---

## Project Structure

A QBUILD project is defined using a single `.bld` file in the root directory.

### Example

```
[name]
MYPROJ

[qb]
main.bas
util.bas

[asm]
math.asm
io.asm
```

---

## Directory Layout (Generated)

```
build\
  obj\
    asm\
    qb\
  bin\
  lib\
  project.mak
  build.log
  step.rsp

launch.bat
```

### Directory Purpose

| Directory         | Purpose                                   |
| ----------------- | ----------------------------------------- |
| build\obj\asm     | Assembly object files                     |
| build\obj\qb      | BASIC object files                        |
| build\lib         | Generated Quick Libraries (.QLB)          |
| build\bin         | Final executable output                   |
| build\project.mak | QuickBASIC IDE project file               |
| launch.bat        | Opens QB with correct library and project |

---

## Build Phases

### 1. Clean Phase

The build directory is cleaned before every build to prevent stale artifacts.

### 2. Assemble (TASM)

Each `.ASM` file is assembled into:

```
build\obj\asm\<name>.obj
build\obj\asm\<name>.lst
```

TASM is executed using a response file:

```
@build\step.rsp
```

The full expanded command is logged to `build\build.log`.

---

### 3. Compile BASIC (BC)

Each `.BAS` file is compiled into:

```
build\obj\qb\<name>.obj
build\obj\qb\<name>.lst
```

BC does **not** support response files.

Command length is validated against the DOS 127-character limit before execution.

---

### 4. Build Quick Library (.QLB)

All ASM object files are merged into a single Quick Library:

```
build\lib\asm.qlb
```

This step uses:

```
LINK /Q
```

The `/Q` switch instructs LINK to generate a Quick Library instead of an EXE.

Link inputs:

* All ASM object files
* `BQLB45.LIB`

---

### 5. Link Executable

All BASIC and ASM object files are linked into:

```
build\bin\<project>.exe
```

Link inputs:

* BASIC objects
* ASM objects
* `BCOM45.LIB`

---

### 6. Generate project.mak

QBUILD generates:

```
build\project.mak
```

Contents:

```
main.bas
util.bas
```

This allows QuickBASIC to load multiple modules automatically.

---

### 7. Generate launch.bat

Created in project root:

```
launch.bat
```

Contents:

```
"C:\QB45\QB.EXE" /l build\lib\asm.qlb build\project.mak
```

This launches QB with:

* The generated Quick Library
* All project modules loaded

---

## Logging

All tool output is appended to:

```
build\build.log
```

For response file steps:

* The response file content is logged
* The expanded equivalent command line is logged

---

## Error Handling

QBUILD performs validation after every major step:

* Object file existence after compilation
* QLB existence after library build
* EXE existence after link
* project.mak existence after generation

If any required output file is missing, execution stops immediately.

---

## DOS Limitations Handled

* 8.3 filename compliance
* 127-character command-line limit
* Single response file reuse (`build\step.rsp`)
* No reliance on unsupported DOS redirection syntax

---

## Toolchain Requirements

Configured via:

```
C:\qbuild\qbuild.ini
```

Example:

```
TASM_PATH=C:\TASM\BIN
QB45_PATH=C:\QB45
```

Required tools:

* TASM.EXE
* BC.EXE
* LINK.EXE
* BQLB45.LIB
* BCOM45.LIB

---

## Design Goals

* Deterministic builds
* Clean directory separation
* IDE + command-line parity
* Historically correct QuickBASIC workflow
* Safe operation under real DOS

---

## Summary

QBUILD provides a structured, automated build system for QuickBASIC 4.5 projects that:

* Supports modular BASIC programs
* Supports external assembly integration
* Generates Quick Libraries for IDE debugging
* Produces standalone executables
* Maintains compatibility with DOS-era constraints

It bridges the gap between the interactive QB IDE and command-line toolchain automation while preserving period-correct behavior.
