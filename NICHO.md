# Ferrum's niche

> **Ferrum replaces bash scripts that outgrew bash.**

This document defines who Ferrum is for, what it is for, and, just as
important, what it is not for. A language that tries to serve everyone serves
no one.

## The problem

Bash is everywhere and it is the right tool for ten lines of glue. Then the
script grows. Quoting rules start to bite, everything is a string, error
handling is a convention instead of a feature, there is no real type checking,
and nobody dares to refactor a 400-line deploy script.

The usual escape hatches each have a cost:

- **Python** is pleasant, but needs an interpreter and its environment on the
  target machine.
- **Go** produces self-contained binaries, but brings a runtime with a garbage
  collector and larger binaries.
- **Rust** is excellent, but its ownership model is a heavy price for a
  200-line internal tool.
- **Zig** is the closest in spirit. Ferrum borrows its explicitness and
  deterministic memory, and adds Python-like syntax and a deliberately small
  surface.

## The niche

- CI/CD pipelines
- Deployment and release tooling
- Internal tools
- Log processing
- Environment bootstrap

These are programs that are short, run often, must start fast, ship as a
single file, and are maintained by people who have other jobs.

## What Ferrum offers

- Small native binaries (target: ~200KB for typical scripts).
- Fast startup (target: under 10ms).
- Python-like syntax that a bash user can read on day one.
- Deterministic memory management: `cofre` / `libera` / `exitus`.
- No surprises: explicit conversions, strict booleans, no null outside `?T`.
- An honest, small stdlib focused on scripting: io, fs, os, proc, path, str
  (Phase 3).

## Success criterion

Phase 3 is done when this holds:

> A 20-line script that processes `.log` files compiles to a binary under
> 500KB, boots in under 10ms, and does not use `audax`.

If Ferrum cannot do that, nothing else about it matters.

## Honesty about limits

Ferrum does **not** promise memory safety.

- No borrow checker. Use-after-free, dangling pointers and double frees are
  possible.
- In Debug builds, a registry of live `cofre` allocations catches double frees
  with a trap. In Release builds the registry is absent and a double free is
  undefined behavior.
- `*T` is non-null by contract, but it may dangle.
- Anything riskier (FFI, `rawptr`, `?*U`) must be written inside an explicit
  `audax { ... }` block, so the risk is visible in the source.
- There is no garbage collector, and there never will be an optional one.
- Linux x86_64 comes first. macOS and Windows arrive in Phase 5.

## Where Ferrum is the wrong tool

- Web services and long-lived servers (no `async` before Phase 6).
- Data science and numerical computing.
- GUI applications.
- Kernels, drivers and safety-critical software.
- Large teams building large codebases: use a language with a mature
  ecosystem and stronger guarantees.

## Non-goals

The full list is in [SPEC.md](SPEC.md), section 11. In short: exceptions, class
inheritance, macros, implicit conversions, operator overloading, optional GC,
`goto`, a full borrow checker and advanced generic features will not be added.
