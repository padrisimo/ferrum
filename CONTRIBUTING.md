# Contributing to Ferrum

Thanks for your interest. Ferrum is a small language with a deliberately narrow
niche (see [NICHO.md](NICHO.md)), so the most useful contributions are usually
the boring ones: tests, bug reports with minimal reproductions, and sharp
edges in the spec that need to be pointed out.

Repository: <https://github.com/padrisimo/ferrum>

## Ground rules

1. **SPEC.md is frozen at v0.4.0.** No new language features without a formal
   spec revision (see "Changing the spec" below). Feature creep kills language
   projects.
2. **Honesty about limits.** Ferrum documents what it does not do. A change
   that makes the docs promise more than the compiler delivers will be
   rejected.
3. **Some things will never be implemented:** `try`/`catch` exceptions, class
   inheritance, preprocessor macros, implicit conversions, operator
   overloading, optional GC, `null` without `?T`, `goto`, pointer arithmetic in
   syntax, booleans as integers, a full borrow checker, `impl Trait`/`dyn
   Trait`, HKT/GATs. Please don't open PRs for these.
4. **No dependencies** beyond LLVM and the C++20 standard library.

## Code

- Strict C++20 (`CMAKE_CXX_EXTENSIONS OFF`).
- Warnings are errors: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror`.
- Debug builds run with `-fsanitize=address,undefined -fno-omit-frame-pointer`.
  Run the test suite in Debug before opening a PR.
- Format with `clang-format` (the repo contains a `.clang-format`).
- Compiler error messages are written in **English** and carry `line:column`.
  Example programs may use Spanish identifiers (`archivo`, `contenido`): that
  is the domain language of the niche.
- Documentation is English-first.

## Building and testing

You need CMake 3.20 or newer and a C++20 compiler (clang or gcc).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

To add a test, create `tests/test_<name>.cpp`: a plain `main()` that returns
`0` on success. It is built and registered with `ctest` automatically (see
`tests/README.md`).

## Commits

- **Atomic:** one logical change per commit.
- **Always green:** every commit builds and passes the tests. Never leave the
  repository broken between commits.
- **[Conventional Commits](https://www.conventionalcommits.org/):**

  ```
  feat(lexer): tokenize range operators
  fix(parser): reject chained comparisons
  docs(spec): clarify exitus evaluation order
  ```

  Common scopes: `lexer`, `parser`, `ast`, `sema`, `codegen`, `cli`, `spec`,
  `docs`, `build`, `test`.

## Pull requests

Fill in the PR template. Keep PRs small and focused. If a PR needs more than a
paragraph to justify, it probably needs an issue first.

## Changing the spec

1. Open a feature request issue describing a concrete problem in a concrete
   script, not a general wish.
2. If accepted, the change goes in as a dedicated, reviewed spec revision that
   bumps the version in SPEC.md and records the change in its changelog.
3. Only then does implementation work start.

## Reporting bugs

Use the bug report template. A minimal `.fe` file plus the output of
`--dump-tokens` or `--dump-ast` (when available) is worth a thousand words.

## License

By contributing you agree that your contributions are licensed under the MIT
License of this project.
