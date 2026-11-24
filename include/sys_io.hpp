#ifndef BYTEFLIP_SYS_IO_HPP
#define BYTEFLIP_SYS_IO_HPP

#include "common.hpp"

namespace byteflip {
namespace sys_io {

/// Compute the length of a null-terminated C-string without relying on libc helpers.
size_t cstr_len(const char *s);

/// Return a pointer to the final filename component of a POSIX path.
/// Example: "/tmp/data/input.txt" -> "input.txt"
const char *base_name(const char *path);

/// Parse a base-10 integer string into a 64-bit signed integer with overflow detection.
/// Sets *ok to 1 on success, or 0 if the string is empty, malformed, or overflows.
long long parse_int64(const char *s, int *ok);

/// Write exactly `count` bytes to `fd`, retrying on partial writes and EINTR.
/// Returns 0 on success, -1 on error.
int write_all(int fd, const char *buf, size_t count);

/// Write a null-terminated string to `fd` using write_all().
int write_str(int fd, const char *s);

/// Read up to `count` bytes from `fd`, retrying on partial reads and EINTR.
/// Stores the actual bytes read in `*got`. Returns 0 on success, -1 on error.
int read_full(int fd, char *buf, size_t count, size_t *got);

/// Ensure `dir_path` exists as a directory with strict user-only permissions (0700).
/// Returns 0 on success, -1 on error.
int ensure_output_dir(const char *dir_path);

/// Build "<dir_path>/<mode_digit>_<basename>" into `out_buf` (size `buf_size`).
/// Returns 0 on success, -1 if the resulting path exceeds `buf_size`.
int build_output_path(char *out_buf, size_t buf_size,
                      const char *dir_path, int mode_digit,
                      const char *input_basename);

}  // namespace sys_io
}  // namespace byteflip

#endif  // BYTEFLIP_SYS_IO_HPP
