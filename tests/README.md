# Tests

Every `tests/test_*.cpp` file is built as its own executable and registered
with `ctest` automatically. A test is a plain `main()` that returns `0` on
success and non-zero on failure; there is no test framework dependency.
