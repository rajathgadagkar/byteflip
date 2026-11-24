# ByteFlip

**Low-Level POSIX System-Call File Reversal & Integrity Verification Toolkit**

`C++17` · `POSIX (Linux / macOS)` · `1 MiB Fixed Buffer` · `64-Bit Large-File Support` · `MIT License`

> A zero-dependency, constant-memory C++ toolkit built exclusively on POSIX system calls for reversing multi-gigabyte files across multiple strategies and auditing byte-level integrity and file permissions.

## Overview

**ByteFlip** is a systems-level file transformation and verification suite designed to process files larger than available physical RAM. Rather than relying on C++ I/O streams, STL containers, or shell commands, every operation—from directory creation and permission enforcement to chunked reverse seeking and live terminal progress reporting—is implemented directly over raw POSIX system calls.

### Key Features

- **Constant-Memory Streaming (`O(1)` RAM):** Operates with a fixed **1 MiB** reusable buffer and 64-bit offsets (`_FILE_OFFSET_BITS=64`), handling multi-gigabyte files without memory spikes.
- **Three Reversal Strategies:** Supports **block-wise**, **full-file**, and **partial-range** byte reversal with exact 0-based index control.
- **Real-Time In-Place Progress:** Overwrites a single console line (`\rWritten: XX.XX%`) using raw `write()` calls without scrolling clutter.
- **Strict Permission Enforcement:** Creates the output directory with `0700` permissions and output files with `0600` permissions, enforced via `chmod`/`fchmod` regardless of process `umask`.
- **Independent Streaming Verifier:** Includes `byteflip-verify` to validate byte-for-byte reversal accuracy across all three modes and emit a 30-line POSIX permission audit for the output file, input file, and directory.
- **Built-In Benchmark Generator:** Includes `byteflip-datagen` to synthesize multi-megabyte or multi-gigabyte random datasets with live throughput reporting.

## Reversal Modes

### Mode `0` — Block-Wise Reversal
Reverses each fixed-size block independently while preserving block order (any trailing partial block is reversed in place).

```text
Input  (block = 4):  [ A B C D ] [ E F G H ]
                       <----->     <----->
Output (0_file):     [ D C B A ] [ H G F E ]
```

### Mode `1` — Full-File Reversal
Globally reverses every byte in the file by seeking backward from `EOF` in 1 MiB chunks, reversing each chunk in place, and streaming sequentially to the output file.

```text
Input:               H e l l o ,   W o r l d !
                     <----------------------->
Output (1_file):     ! d l r o W   , o l l e H
```

### Mode `2` — Partial-Range Reversal
Reverses the prefix `[0, start - 1]` and suffix `[end + 1, EOF]` while preserving the inclusive range `[start, end]` unchanged.

```text
Input  (start=2, end=7):  [ H e ] [ l l o ,   W ] [ o r l d ! ]
                           <--->     unchanged      <------->
Output (2_file):          [ e H ] [ l l o ,   W ] [ ! d l r o ]
```

## Architecture & Memory Model

```text
+---------------------------+        +------------------------------+
|         byteflip          |        |       byteflip-verify        |
|   src/reverser/main.cpp   |        |    src/verifier/main.cpp     |
+-------------+-------------+        +--------------+---------------+
              |                                     |
              v                                     v
+---------------------------+        +------------------------------+
|      Reversal Engine      |        |     Verification Engine      |
| src/reverser/reverser.cpp |        |  src/verifier/verifier.cpp   |
| - reverse_range()         |        |  - check_reversed_range()    |
| - copy_range()            |        |  - check_direct_range()      |
| - print_progress()        |        |  - print_permissions()       |
+-------------+-------------+        +--------------+---------------+
              |                                     |
              +------------------+------------------+
                                 v
              +-------------------------------------+
              |       Shared POSIX I/O Layer        |
              |        src/common/sys_io.cpp        |
              |  - read_full() / write_all()        |
              |  - parse_int64() (overflow-safe)    |
              |  - ensure_output_dir() / path utils |
              +-------------------------------------+
                                 |
                                 v
      POSIX Kernel Syscalls: open, read, write, lseek, stat,
                             fstat, mkdir, chmod, fchmod, close
```

| Metric | Reverser (`byteflip`) | Verifier (`byteflip-verify`) |
| :--- | :--- | :--- |
| **Time Complexity** | `O(N)` — single pass over all `N` bytes | `O(N)` — single dual-stream pass over `N` bytes |
| **Memory Footprint** | **1 MiB** fixed buffer (`O(1)`) | **2 MiB** fixed buffers (`O(1)`) |
| **Large-File Support** | 64-bit offsets (`_FILE_OFFSET_BITS=64`) | 64-bit offsets (`_FILE_OFFSET_BITS=64`) |
| **Partial I/O & `EINTR`** | Handled via `read_full` / `write_all` retry loops | Handled via `read_full` / `write_all` retry loops |

## Project Structure

```text
byteflip/
├── Makefile                    # Modular C++17 build & test automation
├── README.md                   # Project documentation
├── LICENSE                     # MIT License
├── .gitignore                  # Build, binary, and output exclusions
├── include/
│   ├── common.hpp              # Shared constants, buffer sizes, and mode enums
│   ├── sys_io.hpp              # Low-level POSIX I/O & safe integer parsing API
│   ├── reverser.hpp            # Reversal configuration & engine interface
│   └── verifier.hpp            # Integrity & permission verification interface
├── src/
│   ├── common/
│   │   └── sys_io.cpp          # Resilient syscall wrappers & path helpers
│   ├── reverser/
│   │   ├── reverser.cpp        # Streaming block, full, and partial reversal logic
│   │   └── main.cpp            # CLI parser & entry point for `byteflip`
│   └── verifier/
│       ├── verifier.cpp        # Dual-stream content check & permission auditor
│       └── main.cpp            # CLI parser & entry point for `byteflip-verify`
├── tools/
│   └── datagen.cpp             # Large-file random dataset generator
└── tests/
    └── run_tests.sh            # End-to-end automated verification suite
```

## Getting Started

### Prerequisites

- A POSIX-compliant OS (Linux or macOS)
- `g++` (or `clang++`) with C++17 support
- `make`

### Build

```bash
make
```

This compiles three executables into `build/bin/`:
- `build/bin/byteflip` — File reversal utility
- `build/bin/byteflip-verify` — Content & permission verifier
- `build/bin/byteflip-datagen` — Random test-file generator

To remove all build and output artifacts:

```bash
make clean
```

## Usage

### 1. Generate a Sample File (Optional)

```bash
./build/bin/byteflip-datagen sample.bin 10000000   # Generates a 10 MB random file
```

> **Note:** `1000000000` bytes (~1 GB) is supported for large-file stress testing; avoid opening multi-gigabyte test files in text editors.

### 2. Reverse Files with `byteflip`

All transformed files are written into `output/<mode>_<basename>` with directory mode `0700` and file mode `0600`.

```bash
# Mode 0: Block-wise reversal (e.g., 4096-byte blocks)
./build/bin/byteflip sample.bin 0 4096

# Mode 1: Full-file reversal
./build/bin/byteflip sample.bin 1

# Mode 2: Partial-range reversal (0-based inclusive range kept unchanged)
./build/bin/byteflip sample.bin 2 1000 5000
```

### 3. Verify Integrity & Permissions with `byteflip-verify`

```bash
# Verify Mode 0
./build/bin/byteflip-verify output/0_sample.bin sample.bin output 0 4096

# Verify Mode 1
./build/bin/byteflip-verify output/1_sample.bin sample.bin output 1

# Verify Mode 2
./build/bin/byteflip-verify output/2_sample.bin sample.bin output 2 1000 5000
```

#### Sample Verification Output

```text
Directory is created: Yes
Whether file contents are correctly processed: Yes
Both Files Sizes are Same: Yes
User has read permissions on newfile: Yes
User has write permission on newfile: Yes
User has execute permission on newfile: No
Group has read permissions on newfile: No
Group has write permission on newfile: No
Group has execute permission on newfile: No
Others has read permissions on newfile: No
Others has write permission on newfile: No
Others has execute permission on newfile: No
... (followed by identical 9-bit audits for oldfile and directory)
```

## Automated Testing

Run the full end-to-end test suite (covering all 3 modes, empty files, multi-MiB streaming across buffer boundaries, strict `0700`/`0600` permission checks, and CLI error handling):

```bash
make test
```

## License

This project is licensed under the [MIT License](LICENSE).
