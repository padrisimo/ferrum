# Ferrum ABI

**Applies to:** language specification v0.4.0
**Status:** draft. **Not stable.** Ferrum makes no binary-compatibility promise
between compiler versions until Phase 5, when C FFI and modules land.

This document describes how Ferrum values are laid out in memory and how
functions are called. It exists so that the compiler, the C FFI and the
documentation agree on one answer.

## 1. Targets

| Target | Phase |
|---|---|
| Linux x86_64 | first target (Phases 2-4) |
| macOS, Windows | Phase 5 |

All sizes below assume a 64-bit target.

## 2. Primitive types

| Type | Size (bytes) | Alignment | Notes |
|---|---|---|---|
| `i8`, `u8` | 1 | 1 | |
| `i16`, `u16` | 2 | 2 | |
| `i32`, `u32` | 4 | 4 | |
| `i64`, `u64` | 8 | 8 | |
| `isize`, `usize` | 8 | 8 | pointer-sized |
| `f32` | 4 | 4 | IEEE 754 binary32 |
| `f64` | 8 | 8 | IEEE 754 binary64 |
| `bool` | 1 | 1 | only `0` (`falsum`) and `1` (`verum`) are valid |
| `rawptr` | 8 | 8 | opaque address, only inside `audax` |
| `vacuum` | 0 | 1 | no value |

Integers are two's complement and little-endian on the initial targets.
`bool` is never an integer in the language; the 0/1 representation is a layout
fact, not a conversion.

## 3. Pointers and optionals

| Type | Layout |
|---|---|
| `*T` | one machine pointer. Non-null by contract; may be dangling |
| `?*U` | one machine pointer where the null address means `nihil`. Only inside `audax` |
| `?T` | implementation detail in v0.4.0; **not** part of the ABI yet |

`cofre(T, n)` returns a pointer aligned to at least the alignment of `T`.

## 4. Strings and slices

Both are two-word values passed and stored by value:

```
str   = { ptr: *u8, len: usize }     # 16 bytes, UTF-8, NOT NUL-terminated
[]T   = { ptr: *T,  len: usize }     # 16 bytes
```

`len` counts elements (bytes for `str`). When a `str` crosses the C boundary,
the caller is responsible for producing a NUL-terminated copy where C expects
one; Ferrum never assumes a trailing NUL.

## 5. Composite types

`forma` (Phase 4) is laid out like a C struct: fields in declaration order,
each at its natural alignment, total size rounded up to the struct's alignment.
This is what makes `foris` (extern) declarations of C structs possible.

Tuples are reserved and have no layout yet.

## 6. Calling convention

- Ferrum functions and `foris` (extern) functions use the platform's C calling
  convention (System V AMD64 on Linux x86_64).
- `str` and `[]T` follow the C rules for a two-word aggregate.
- A function without a declared return type returns `vacuum`.

## 7. Deferred

The following are fixed by the phase that introduces them and are not part of
this draft:

- Symbol naming and mangling (Phase 5, with modules)
- Representation of `?T` for non-pointer `T` (Phase 2)
- Program entry point and exit status (Phase 3, with the stdlib)
- `Owned[T]` layout (Phase 4)
