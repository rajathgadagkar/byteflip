#define _FILE_OFFSET_BITS 64

#include "sys_io.hpp"

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

namespace byteflip {
namespace sys_io {

size_t cstr_len(const char *s) {
    if (s == nullptr) return 0;
    size_t n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

const char *base_name(const char *path) {
    if (path == nullptr) return "";
    size_t len = cstr_len(path);
    if (len == 0) return path;

    size_t i = len;
    while (i > 0 && path[i - 1] != '/') --i;
    return path + i;
}

long long parse_int64(const char *s, int *ok) {
    long long value = 0;
    int sign = 1;
    size_t i = 0;
    *ok = 0;

    if (s == nullptr || s[0] == '\0') return 0;
    if (s[0] == '-') {
        sign = -1;
        i = 1;
    }
    if (s[i] == '\0') return 0;

    for (; s[i] != '\0'; ++i) {
        if (s[i] < '0' || s[i] > '9') return 0;
        int digit = s[i] - '0';
        if (value > (0x7fffffffffffffffLL - digit) / 10) return 0;
        value = value * 10 + digit;
    }

    *ok = 1;
    return value * sign;
}

int write_all(int fd, const char *buf, size_t count) {
    size_t done = 0;
    while (done < count) {
        ssize_t n = write(fd, buf + done, count - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        done += static_cast<size_t>(n);
    }
    return 0;
}

int write_str(int fd, const char *s) {
    return write_all(fd, s, cstr_len(s));
}

int read_full(int fd, char *buf, size_t count, size_t *got) {
    size_t done = 0;
    while (done < count) {
        ssize_t n = read(fd, buf + done, count - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        done += static_cast<size_t>(n);
    }
    *got = done;
    return 0;
}

int ensure_output_dir(const char *dir_path) {
    struct stat st;

    if (mkdir(dir_path, 0700) < 0 && errno != EEXIST) {
        perror("mkdir");
        return -1;
    }

    if (stat(dir_path, &st) < 0) {
        perror("stat");
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        write_str(STDERR_FILENO, "Error: output path exists and is not a directory.\n");
        return -1;
    }

    if (chmod(dir_path, 0700) < 0) {
        perror("chmod");
        return -1;
    }
    return 0;
}

int build_output_path(char *out_buf, size_t buf_size,
                      const char *dir_path, int mode_digit,
                      const char *input_basename) {
    size_t dir_len = cstr_len(dir_path);
    size_t name_len = cstr_len(input_basename);
    int needs_slash = (dir_len > 0 && dir_path[dir_len - 1] != '/') ? 1 : 0;

    // "<dir_path>[/]<digit>_<basename>\0"
    size_t total_len = dir_len + static_cast<size_t>(needs_slash) + 2 + name_len + 1;
    if (total_len > buf_size) return -1;

    size_t pos = 0;
    for (size_t i = 0; i < dir_len; ++i) out_buf[pos++] = dir_path[i];
    if (needs_slash) out_buf[pos++] = '/';
    out_buf[pos++] = static_cast<char>('0' + mode_digit);
    out_buf[pos++] = '_';
    for (size_t i = 0; i < name_len; ++i) out_buf[pos++] = input_basename[i];
    out_buf[pos] = '\0';

    return 0;
}

}  // namespace sys_io
}  // namespace byteflip
