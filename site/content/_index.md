+++
title = "bwapi-c2"
description = "A flat C ABI over BWAPI and BWEM, so any language with a C FFI can drive a StarCraft: Brood War bot."
sort_by = "weight"
+++

**bwapi-c2** is a flat C ABI over [BWAPI](https://github.com/bwapi/bwapi), the StarCraft: Brood
War bot API, and [BWEM](https://github.com/N00byEdge/BWEM-community), its map analysis library.
One DLL, three headers, no C++ at the call site: any language with a C foreign-function interface
can drive a bot with it.

> **Status: unstable, pre-1.0.** The library is being built in the open, phase by phase. Nothing
> here is a release yet; the ABI is `0.x` and may change until the consumer phase completes.

## The problem

BWAPI is consumable only from C++. Everything a bot touches is a C++ class with virtual dispatch,
`std::string` and `std::unordered_set` return values, `std::function` predicates and printf-style
varargs. None of that crosses a foreign-function boundary. Three things have been tried about it:
two languages reimplemented the client protocol and are thriving on it (Java and Rust); one C ABI
existed, reached 1.0 in 2018, and stopped; and some twenty other attempts, in C#, Python, Lua, Go,
Zig, Nim and Haskell, died.

## Why a fourth attempt goes differently

- **It is for the languages with no maintained option.** Python and C# first, then everything an
  `api.json` file can reach. Java and Rust are served; this is not a third Rust binding.
- **It is built over the real `BWAPI::Game`.** The two protocol reimplementations each ported
  BWAPI's shared game rules and static type data by hand: 72% and 86% of their codebases, with
  fourteen recorded divergence bugs between them, and a fix upstream fixes nothing downstream. A C
  ABI over the real implementation never ports them at all.
- **It ships the two things the previous C ABI did not:** the static type database, and a license.

And it wraps BWEM through a second header, because a bot without map analysis starts behind every
C++ bot on day one.

## What you get

| Artifact | What it is |
|---|---|
| `bwapi_c2.dll` | The library, for Win32 and x64, consumed dynamically |
| `bwapi_c2.h`, `bwapi_c2_bwem.h`, `bwapi_c2_types.h` | Three C99 headers; the whole surface |
| `api.json` | A machine-readable description of every export, for binding generators |

The library is **LGPL-3.0-only**, because it contains BWAPI's object code. Your bot's own code
stays yours; your *distribution* carries the license files and a notice. [What the license asks
of a bot author](@/explanation/license-for-bot-authors.md) says exactly what.

## The documentation

Four sections, each with one job.

- **[Tutorials](@/tutorials/_index.md)**: learning by doing, one path, guaranteed to work.
- **[How-to guides](@/how-to/_index.md)**: a task you already have, done.
- **[Reference](@/reference/_index.md)**: the facts, one page per function, generated from the
  same source as the headers.
- **[Explanation](@/explanation/_index.md)**: why it is the way it is.

The design record, a plan of some two thousand lines and eleven rounds of research, lives in the
[repository](https://github.com/RadicalZephyr/bwapi-c2/tree/main/docs). Explanation pages
condense it and link to the full argument.
