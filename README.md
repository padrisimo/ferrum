# Ferrum

**A compiled language for replacing bash scripts that outgrew bash.**

Ferrum compiles to small native binaries (~200KB), boots in under 10ms, reads
like Python, manages memory deterministically, and uses a 100% Latin
vocabulary.

> **Ferrum is not Rust.** There is no borrow checker and no automatic
> memory-safety guarantee. Think of it as *Zig with Python-like syntax and
> Latin keywords*. What Ferrum promises, it delivers; what it doesn't, it says
> so up front. See [NICHO.md](NICHO.md).

## Status

**Phase 1 of 6 is in progress** (lexer, AST, parser). Specification v0.4.0 is
frozen. Nothing compiles to a binary yet, and the build system is not in the
repository yet. Everything below describes the target, not the present.

## The niche

CI/CD, deployment, internal tools, log processing and environment bootstrap:
the places where bash starts to hurt, but Python, Go or Rust are overkill.
The full definition, including where Ferrum is *not* the right tool, is in
[NICHO.md](NICHO.md).

## A taste of the syntax

A sketch of the Phase 1 syntax (it will parse; it does not compile yet):

```
opus suma(a: i32, b: i32) -> i32 {
    redde a + b
}

opus main() -> i32 {
    fixum buffer = cofre(u8, 4096)   # typed heap allocation
    exitus libera(buffer)            # deferred; runs when the block ends

    mut intentos = 0
    dum intentos < 3 {
        intentos = intentos + 1
    }

    si suma(intentos, 1) > 3 {
        redde 0
    } aliter {
        redde 1
    }
}
```

The vocabulary is small: 25 keywords, all Latin. The complete list, the type
system, the memory model and the grammar live in [SPEC.md](SPEC.md).

## Principles

- **Honesty about limits.** No promise the compiler cannot keep.
- **Explicit over implicit.** Explicit `mut`, explicit conversions with `ut`,
  strict booleans, `nihil` only inside `?T`.
- **Deterministic memory.** `cofre` allocates, `libera` frees, `exitus` defers.
  No GC. Risky operations live inside an explicit `audax { ... }` block.
- **Small.** Few keywords, few concepts, no dependencies beyond LLVM and the
  C++20 standard library.

## Roadmap

| Phase | Content | Status |
|---|---|---|
| 1 | Lexer, AST, parser | in progress |
| 2 | Semantic analysis, `?T`, LLVM codegen, `fmt`, `fix` | planned |
| 3 | Scripting stdlib (io/fs/os/proc/path/str), `per`, `dum`, `exitus` | planned |
| 4 | `forma`, tuples, `audax`, `Owned[T]`, `doc` | planned |
| 5 | C FFI, modules, `genus`, generics, macOS/Windows | planned |
| 6 | `discerne`, `async`, `const fn`, LSP | planned |

The first cycle is a trimmed MVP: Linux x86_64 only, no `async`, no generics,
no `genus`, no tuples. The Phase 3 goal: a 20-line script that processes
`.log` files, compiles to a binary under 500KB, boots in under 10ms, and does
not need `audax`.

## Repository layout

```
SPEC.md             Language specification (frozen at v0.4.0)
NICHO.md            Niche definition: who Ferrum is for, and who it is not for
ABI.md              Data layout and calling conventions
CONTRIBUTING.md     How to contribute
.github/            Issue and pull request templates
```

Source code (`src/`), tests (`tests/`) and examples (`examples/`) arrive in the
following commits.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) first. The short version: the spec is
frozen, every commit builds and passes tests, and some features will never be
implemented (the list is in SPEC.md, section 11).

## License

[MIT](LICENSE)
