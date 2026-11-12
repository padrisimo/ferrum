# Ferrum Language Specification

**Version:** 0.4.0
**Status:** frozen. Phase 1 (lexer, AST, parser) is implemented against this
document. No new features without a formal spec revision (section 14).

Ferrum is a compiled language for replacing bash scripts that outgrew bash. It
compiles to small native binaries, boots in under 10ms, uses Python-like syntax
and a 100% Latin vocabulary, and manages memory deterministically.

**Ferrum is not Rust.** It has no borrow checker and no automatic memory-safety
guarantee. It is Zig with Python-like syntax and Latin keywords.

Phase tags such as **[P2]** mark when a feature gets its semantics. Syntax can
be parsed earlier than it is given meaning; the grammar is in Appendix A.

## Contents

1. Design principles
2. Lexical structure
3. Types
4. Expressions
5. Statements
6. Functions
7. Memory model
8. Modules and visibility
9. Tooling and diagnostics
10. Reserved syntax
11. What will never be implemented
12. Roadmap
13. Deferred details
14. Revision process
- Appendix A: Grammar
- Changelog

---

## 1. Design principles

1. **Honesty about limits.** The language promises only what the compiler
   delivers.
2. **Zig, not Rust.** Deterministic memory, explicit risk, no ownership
   checker.
3. **Explicit over implicit.** Mutability, conversions and risk are always
   visible in the source.
4. **Small.** 25 keywords. Few concepts.
5. **Latin vocabulary.** Every keyword is Latin.
6. **Niche first.** Every decision is judged against replacing outgrown bash
   scripts (see NICHO.md).

## 2. Lexical structure

### 2.1 Source text

Source files use the extension `.fe` and are UTF-8. Line endings are LF or
CRLF. Tokens carry a line and a column for diagnostics.

### 2.2 Whitespace and terminators

Newlines are ordinary whitespace. The semicolon `;` is an **optional**
statement terminator; it is never mandatory.

Because newlines are not significant, a statement that starts with `(`, `[`,
`-`, `*` or `&` can be read as a continuation of the previous expression. When
that matters, end the previous statement with `;`. This is a known limit of
the design.

### 2.3 Comments

A comment starts with `#` and runs to the end of the line. There are no block
comments. `##` is reserved for documentation comments consumed by `doc`
**[P4]**.

### 2.4 Identifiers

An identifier matches `[A-Za-z_][A-Za-z0-9_]*` (ASCII only). Keywords and type
names are reserved and cannot be identifiers.

### 2.5 Keywords

There are 25 keywords, all Latin.

| Category | Keyword | Meaning |
|---|---|---|
| Functions | `opus` | function declaration |
| | `redde` | return |
| | `exitus` | deferred call, LIFO order |
| Variables | `mut` | mutable variable declaration |
| | `fixum` | constant (immutable) declaration |
| | `nihil` | absent value, only inside `?T` |
| | `verum`, `falsum` | boolean literals |
| Control | `si`, `aliter` | if / else |
| | `dum` | while loop |
| | `per`, `in` | for loop |
| | `rumpe` | break |
| | `perge` | continue |
| | `discerne` | reserved for pattern matching **[P6]** |
| Types | `forma` | struct declaration (parsed in P1, semantics **[P4]**) |
| | `genus` | reserved for enums **[P5]** |
| Memory | `cofre` | typed heap allocation |
| | `libera` | frees a `cofre` allocation |
| | `audax` | explicit risk block |
| Modules | `importa` | import **[P5]** |
| | `palam` | public visibility |
| | `foris` | external (FFI) declaration **[P5]** |
| Conversion | `ut` | explicit conversion |

`vacuum` (the empty type) is a primitive type name, not one of the 25 keywords;
it is reserved like the other type names in section 3.

### 2.6 Literals

- **Integers:** decimal `42`, hexadecimal `0xFF`, octal `0o17`, binary `0b1010`.
  `_` may separate digits: `1_000_000`. A literal has no sign and no suffix;
  its type comes from context or from `ut`.
- **Floats:** `3.14`, `1e9`, `2.5e-3`.
- **Strings:** `"text"`, UTF-8, with the escapes `\n \t \r \0 \\ \" \{ \}` and
  `\xHH`. Strings may not contain a raw newline.
- **Booleans:** `verum`, `falsum`.
- **Absent value:** `nihil`.

Interpolation `"{expr}"` is added in **[P3]**. From then on, a literal brace
inside a string is written `\{` or `\}`. The lexer accepts both forms from
Phase 1.

### 2.7 Operators and punctuation

```
+  -  *  /  %
&  |  ^  ~  <<  >>
==  !=  <  <=  >  >=
&&  ||  !
=
..  ..=  .  ,  :  ;  ->  ?
(  )  {  }  [  ]
```

There are no compound assignment operators and no `++`/`--`.

## 3. Types

### 3.1 Primitive types

`i8`, `i16`, `i32`, `i64`, `isize`, `u8`, `u16`, `u32`, `u64`, `usize`, `f32`,
`f64`, `bool`, `str`, `rawptr`, `vacuum`.

- `str` is an immutable UTF-8 byte sequence `{ptr, len}`, not NUL-terminated.
- `rawptr` is an opaque address. It may only be used inside `audax`.
- `vacuum` is the empty type. A function without a return type returns
  `vacuum`.
- `bool` is not an integer. There is no conversion between `bool` and any
  integer type, not even with `ut`.

### 3.2 Composite types

| Syntax | Meaning |
|---|---|
| `*T` | pointer to `T`. Non-null by contract; **may be dangling** |
| `?T` | optional `T`. `nihil` is only a value of `?T` **[P2]** |
| `?*U` | optional pointer. **Forbidden outside `audax`** |
| `[]T` | slice `{ptr, len}` |
| `(T, U)` | tuple. Reserved **[P4]** (section 10) |

### 3.3 Type inference

A binding declared with an initializer may omit its type annotation; the type
is taken from the initializer. Function parameters and return types are always
annotated.

## 4. Expressions

### 4.1 Precedence

From lowest to highest binding power:

| Level | Operators | Associativity |
|---|---|---|
| 1 | `\|\|` | left |
| 2 | `&&` | left |
| 3 | `==` `!=` `<` `<=` `>` `>=` | **none** (see 4.2) |
| 4 | `\|` | left |
| 5 | `^` | left |
| 6 | `&` | left |
| 7 | `<<` `>>` | left |
| 8 | `+` `-` | left |
| 9 | `*` `/` `%` | left |
| 10 | `ut` (conversion) | left |
| 11 | unary `-` `!` `~` `*` (dereference) `&` (address-of) | right |
| 12 | call `f(x)`, index `a[i]`, member `a.b`, propagation `e?` | left |

Bitwise operators bind tighter than comparisons, so `a & b == c` means
`(a & b) == c`.

### 4.2 Chained comparisons are forbidden

`a < b < c` is a syntax error. Write `a < b && b < c`.

### 4.3 Strict booleans

The operands of `&&`, `||`, `!` and the conditions of `si` and `dum` must be of
type `bool`. `si x` with an integer `x` is an error.

### 4.4 Explicit conversion

`expr ut T` converts `expr` to `T`. There is no implicit promotion anywhere:
`i32 + i64` is an error until one side is converted.

- Numeric to numeric conversions are allowed.
- `bool` never converts to or from numbers.
- Conversions involving `rawptr` or pointers are allowed only inside `audax`.

### 4.5 Optional propagation **[P2]**

`e?` where `e` has type `?T`, inside a function returning `?U`, yields the
`T` value or returns `nihil` from the enclosing function.

### 4.6 Ranges **[P3]**

`a..b` is the half-open range `[a, b)`, `a..=b` is the closed range `[a, b]`.
Ranges appear in `per` loops.

### 4.7 String interpolation **[P3]**

`"{expr}"` embeds the textual value of `expr` in a string literal.

### 4.8 Pointers

There is no pointer arithmetic in the syntax. `*p` dereferences and `&x` takes
an address. Stepping a pointer is only possible with `avanza` and `retrocede`
inside `audax` **[P4]**.

## 5. Statements

### 5.1 Variables

```
mut contador = 0           # mutable
fixum limite: i32 = 10     # constant: reassignment is an error
nombre = "ferrum"          # AssignOrDecl: declares an immutable binding
```

- `mut` and `fixum` declare a binding explicitly. Mutability is always
  explicit: a binding is not mutable unless it says `mut`.
- A bare `x = expr` is an *assign-or-declare* statement. If `x` is already in
  scope it is an assignment; otherwise it declares a new binding. Which one it
  is gets decided by semantic analysis **[P2]**; the parser only records that
  it was written without a keyword.
- Assigning to an implicitly declared binding is an auto-fixable **warning**:
  `x = 5` followed by `x = 10` suggests `mut x = 5`, and `ferrumc fix` applies
  it. Assigning to a `fixum` binding is an **error**.
- Assignment through an index, member or dereference (`a[i] = v`, `p.x = v`,
  `*p = v`) is a plain assignment.

### 5.2 Conditionals

```
si x > 0 {
    ...
} aliter si x == 0 {
    ...
} aliter {
    ...
}
```

The condition is a `bool` expression and needs no parentheses. Braces are
mandatory.

### 5.3 Loops

```
dum condicion { ... }
per i in 0..10 { ... }          # [P3]
```

`rumpe` leaves the innermost loop; `perge` continues with its next iteration.

### 5.4 Return

`redde expr` returns a value. `redde` with no expression is only valid when the
next token is `}`, `;` or the end of the file.

### 5.5 Deferred calls

`exitus call(args)` schedules a call for when the enclosing block ends; see
section 7.3.

### 5.6 Blocks

`{ ... }` introduces a scope. `audax { ... }` introduces a scope in which risky
operations are allowed (section 7.5).

## 6. Functions

```
opus suma(a: i32, b: i32) -> i32 {
    redde a + b
}

palam opus saludar(nombre: str) {      # public, returns vacuum
    ...
}
```

- Parameters and the return type are annotated. `-> T` is omitted for `vacuum`.
- `palam` before a declaration makes it visible to other modules **[P5]**.
- `foris opus nombre(a: i32) -> i32` declares a function implemented outside
  Ferrum (C FFI), without a body **[P5]**. Calling it requires `audax`.
- Functions are not overloaded. There are no default arguments.

## 7. Memory model

Memory is managed deterministically, by hand, with explicit syntax. There is no
garbage collector and there never will be one.

### 7.1 Allocation

`cofre(T, n)` allocates space for `n` values of type `T` on the heap and
returns a `*T`. The first argument is a type, the second a `usize`.

### 7.2 Freeing

`libera(p)` frees a pointer obtained from `cofre`. It accepts nothing else.
Freeing the same pointer twice is undefined behavior; in Debug builds it traps.

### 7.3 Deferred calls

`exitus` defers a call to the end of the enclosing block.

- Deferred calls run in **LIFO** order.
- The arguments are evaluated **at the point of the `exitus`**, not when the
  call finally runs.
- They run however the block is left: end of block, `redde`, `rumpe`, `perge`.

```
fixum a = cofre(u8, 64)
exitus libera(a)
fixum b = cofre(u8, 64)
exitus libera(b)      # b is freed first, then a
```

### 7.4 Debug registry

Debug builds keep a registry of live `cofre` allocations, which is how double
frees are caught. Release builds do not have the registry and do not check.

### 7.5 `audax`

`audax { ... }` marks code where Ferrum's usual checks do not apply. It is
required for:

- using `rawptr`,
- using `?*U`,
- calling `foris` (FFI) functions,
- pointer conversions,
- `avanza` and `retrocede` **[P4]**.

The block does not make the code safer; it makes the risk **visible**.

### 7.6 What Ferrum does not guarantee

No protection against use-after-free, dangling pointers, leaks or double frees
beyond the Debug registry. See NICHO.md.

## 8. Modules and visibility

`importa "std/fs"` brings a module into scope **[P5 semantics; parsed in P1]**.
Declarations are private to their module unless marked `palam`. `foris`
declares external symbols (section 6).

## 9. Tooling and diagnostics

- `ferrumc` is the compiler CLI. In Phase 1 it offers `--dump-tokens` and
  `--dump-ast`.
- `ferrumc fmt` formats source **[P2]**.
- `ferrumc fix` applies auto-fixable warnings **[P2]**.
- `ferrumc doc` extracts `##` documentation comments **[P4]**.
- Diagnostics are written in **English** and carry the file, line and column.
- Warnings that have a mechanical correction are marked auto-fixable.

## 10. Reserved syntax

Accepted by the parser only to give a clear error now, so they can be given
meaning later without breaking programs:

- **Tuples** `(T, U)` and tuple expressions **[P4]**.
- **`discerne`** pattern matching **[P6]**.
- **`genus`** enumerations **[P5]**.
- **`async`** is planned for **[P6]**; its keyword will be added by a spec
  revision.

## 11. What will never be implemented

- `try` / `catch` exceptions
- class inheritance
- preprocessor macros
- implicit conversions
- operator overloading
- optional garbage collection
- `null` without `?T`
- `goto`
- pointer arithmetic in the syntax
- booleans as integers
- a full borrow checker
- `impl Trait` / `dyn Trait`
- higher-kinded types and generic associated types

## 12. Roadmap

| Phase | Content |
|---|---|
| 1 | Lexer, AST, parser |
| 2 | Semantic analysis, `?T`, LLVM codegen, `fmt`, `fix` |
| 3 | Scripting stdlib (io/fs/os/proc/path/str), `per`, `dum`, `exitus` |
| 4 | `forma`, tuples, `audax`, `Owned[T]`, `doc` |
| 5 | C FFI, modules, `genus`, generics, macOS/Windows |
| 6 | `discerne`, `async`, `const fn`, LSP |

The trimmed MVP targets Linux x86_64 and has no `async`, no generics, no
`genus`, no tuples and no advanced `?T`.

**Phase 3 success criterion:** a 20-line script that processes `.log` files,
compiles to a binary under 500KB, boots in under 10ms, and does not use
`audax`.

## 13. Deferred details

These details of already-specified features are fixed by the phase that
implements them, and are not part of v0.4.0:

- Integer overflow and division by zero (Phase 2)
- Evaluation order of function arguments (Phase 2)
- Behavior when `cofre` cannot allocate (Phase 2)
- Program entry point, arguments and exit status (Phase 3)
- Symbol naming and module paths (Phase 5)

## 14. Revision process

This document changes only through a dedicated, reviewed revision that bumps
the version and adds an entry to the changelog. Bug fixes in wording that do
not change meaning are allowed without a version bump.

---

## Appendix A: Grammar

Notation: `{ x }` zero or more, `[ x ]` optional, `( a | b )` alternatives,
quoted text is a literal token. This grammar covers the whole language as
parsed in Phase 1.

```
program      = { import_decl | decl } EOF ;

import_decl  = "importa" STRING ;

decl         = [ "palam" ] ( func_decl | struct_decl | const_decl )
             | extern_decl ;

func_decl    = "opus" IDENT "(" [ params ] ")" [ "->" type ] block ;
extern_decl  = "foris" "opus" IDENT "(" [ params ] ")" [ "->" type ] ;
params       = param { "," param } [ "," ] ;
param        = IDENT ":" type ;

struct_decl  = "forma" IDENT "{" [ field { "," field } [ "," ] ] "}" ;
field        = IDENT ":" type ;

const_decl   = "fixum" IDENT [ ":" type ] "=" expr ;

type         = "*" type
             | "?" type
             | "[" "]" type
             | IDENT                 (* primitive type name or forma name *)
             ;                       (* "(" T "," U ")" is reserved: error *)

block        = "{" { stmt } "}" ;

stmt         = ";"
             | var_decl
             | assign_or_decl
             | assign
             | return_stmt
             | exit_stmt
             | "rumpe"
             | "perge"
             | if_stmt
             | while_stmt
             | for_stmt
             | "audax" block
             | block
             | expr ;

var_decl       = ( "mut" | "fixum" ) IDENT [ ":" type ] "=" expr ;
assign_or_decl = IDENT "=" expr ;
assign         = lvalue "=" expr ;       (* lvalue: index, member or dereference *)
return_stmt    = "redde" [ expr ] ;      (* expr absent only before "}", ";" or EOF *)
exit_stmt      = "exitus" call ;         (* stored as { callee, args } *)

if_stmt      = "si" expr block { "aliter" "si" expr block } [ "aliter" block ] ;
while_stmt   = "dum" expr block ;
for_stmt     = "per" IDENT "in" expr block ;

expr         = or_expr ;
or_expr      = and_expr { "||" and_expr } ;
and_expr     = cmp_expr { "&&" cmp_expr } ;
cmp_expr     = bitor_expr [ cmp_op bitor_expr ] ;     (* at most one: no chaining *)
cmp_op       = "==" | "!=" | "<" | "<=" | ">" | ">=" ;
bitor_expr   = bitxor_expr { "|" bitxor_expr } ;
bitxor_expr  = bitand_expr { "^" bitand_expr } ;
bitand_expr  = shift_expr { "&" shift_expr } ;
shift_expr   = add_expr { ( "<<" | ">>" ) add_expr } ;
add_expr     = mul_expr { ( "+" | "-" ) mul_expr } ;
mul_expr     = cast_expr { ( "*" | "/" | "%" ) cast_expr } ;
cast_expr    = unary_expr { "ut" type } ;
unary_expr   = ( "-" | "!" | "~" | "*" | "&" ) unary_expr | postfix_expr ;
postfix_expr = primary { call_args | "[" expr "]" | "." IDENT | "?" } ;
call         = postfix_expr ;            (* must end in a call: callee "(" args ")" *)
call_args    = "(" [ expr { "," expr } [ "," ] ] ")" ;

primary      = INT | FLOAT | STRING | "verum" | "falsum" | "nihil"
             | IDENT
             | "cofre" "(" type "," expr ")"
             | "libera" "(" expr ")"
             | "(" expr ")" ;
```

---

## Changelog

### 0.4.0

- Vocabulary fixed at 25 Latin keywords: `fixum` (not `const`), `genus` (not
  `enum`), `palam` (not `pub`), `foris` (not `extern`), `vacuum` (not `void`).
- `cofre` replaces `alloca`; `audax` added.
- Explicit `mut` confirmed; `?T` lands in Phase 2; generics in Phase 5; `async`
  in Phase 6; a borrow checker never.
- Chained comparisons forbidden; explicit conversions with `ut`; strict
  booleans.
