# Welt Language Test Suite

This directory contains tests for the Welt programming language.

## Test Files

### Welt Language Tests (.wt files)
- `test_variables.wt` - Variable declaration and reassignment
- `test_hex_literals.wt` - Hexadecimal literal support
- `test_strings.wt` - String operations
- `test_ss_type.wt` - Auto-detect type system
- `test_print.wt` - Print function
- `test_bit_constraints.wt` - Integer bit constraints (i8bit, u16bit, etc.)
- `test_comments.wt` - Comment handling

### Python Tests
- `test_fsal.py` - Tests for FSAL compiler and module loader

## Running Tests

### All Tests
```bash
python tests/run_tests.py
```

### Individual Welt Test
```bash
welt tests/test_variables.wt
```

## Prerequisites

1. **For Welt tests**: Compile the Welt interpreter from `src/main.c`
```bash
gcc src/main.c -o welt
```

2. **For Python tests**: No additional dependencies required (uses standard library)

## Expected Output

Each test file should execute without errors and produce output showing the test results. The test runner will summarize passed/failed tests at the end.
