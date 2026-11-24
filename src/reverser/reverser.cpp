#define _FILE_OFFSET_BITS 64

#include "reverser.hpp"
#include "sys_io.hpp"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

namespace byteflip {

namespace {

/// Copy byte range [start, end) sequentially from `ifd` to `ofd` using bounded memory.
int copy_range(int ifd, int ofd, off_t start, off_t end, off_t total,
               off_t *written, char *buf) {
    off_t pos = start;
    while (pos < end) {
        off_t remain = end - pos;
        size_t want = (remain > static_cast<off_t>(kBufferSize))
                          ? kBufferSize
                          : static_cast<size_t>(remain);

        if (lseek(ifd, pos, SEEK_SET) == static_cast<off_t>(-1)) {
            perror("lseek");
            return -1;
        }

        size_t got = 0;
        if (sys_io::read_full(ifd, buf, want, &got) < 0 || got != want) {
            perror("read");
            return -1;
        }

        if (sys_io::write_all(ofd, buf, got) < 0) {
            perror("write");
            return -1;
        }

        pos += static_cast<off_t>(got);
        *written += static_cast<off_t>(got);
        print_progress(*written, total);
    }
    return 0;
}

/// Reverse byte range [start, end) from right to left in `kBufferSize` chunks
/// and append the reversed bytes sequentially to `ofd`.
int reverse_range(int ifd, int ofd, off_t start, off_t end, off_t total,
                  off_t *written, char *buf) {
    off_t right = end;

    while (right > start) {
        off_t left = right - static_cast<off_t>(kBufferSize);
        if (left < start) left = start;
        size_t want = static_cast<size_t>(right - left);

        if (lseek(ifd, left, SEEK_SET) == static_cast<off_t>(-1)) {
            perror("lseek");
            return -1;
        }

        size_t got = 0;
        if (sys_io::read_full(ifd, buf, want, &got) < 0 || got != want) {
            perror("read");
            return -1;
        }

        for (size_t i = 0; i < got / 2; ++i) {
            char tmp = buf[i];
            buf[i] = buf[got - 1 - i];
            buf[got - 1 - i] = tmp;
        }

        if (sys_io::write_all(ofd, buf, got) < 0) {
            perror("write");
            return -1;
        }

        *written += static_cast<off_t>(got);
        print_progress(*written, total);
        right = left;
    }
    return 0;
}

/// Mode 0: Reverse every fixed-size block independently while preserving block order.
int reverse_block_wise(int ifd, int ofd, off_t size, off_t block_size, char *buf) {
    off_t pos = 0;
    off_t written = 0;

    while (pos < size) {
        off_t remain = size - pos;
        off_t current_block = (remain < block_size) ? remain : block_size;
        if (reverse_range(ifd, ofd, pos, pos + current_block, size, &written, buf) < 0) {
            return -1;
        }
        pos += current_block;
    }
    return 0;
}

/// Mode 1: Reverse the entire file globally as one continuous range.
int reverse_full_file(int ifd, int ofd, off_t size, char *buf) {
    off_t written = 0;
    return reverse_range(ifd, ofd, 0, size, size, &written, buf);
}

/// Mode 2: Reverse prefix [0, start), preserve [start, end], and reverse suffix (end, size).
int reverse_partial_range(int ifd, int ofd, off_t size, off_t start, off_t end, char *buf) {
    off_t written = 0;

    if (reverse_range(ifd, ofd, 0, start, size, &written, buf) < 0) return -1;
    if (copy_range(ifd, ofd, start, end + 1, size, &written, buf) < 0) return -1;
    if (reverse_range(ifd, ofd, end + 1, size, size, &written, buf) < 0) return -1;

    return 0;
}

}  // namespace

void print_progress(off_t written, off_t total) {
    char out[64];
    int pos = 0;
    double pct = 100.0;

    if (total > 0) {
        pct = (static_cast<double>(written) * 100.0) / static_cast<double>(total);
        if (pct > 100.0) pct = 100.0;
        if (pct < 0.0) pct = 0.0;
    }

    int whole = static_cast<int>(pct);
    int decimals = static_cast<int>((pct - static_cast<double>(whole)) * 100.0 + 0.5);

    if (decimals >= 100) {
        decimals = 0;
        ++whole;
        if (whole > 100) whole = 100;
    }

    const char label[] = "\rWritten: ";
    for (size_t i = 0; i < sizeof(label) - 1; ++i) out[pos++] = label[i];

    if (whole >= 100) {
        out[pos++] = '1';
        out[pos++] = '0';
        out[pos++] = '0';
    } else if (whole >= 10) {
        out[pos++] = static_cast<char>('0' + (whole / 10));
        out[pos++] = static_cast<char>('0' + (whole % 10));
    } else {
        out[pos++] = static_cast<char>('0' + whole);
    }

    out[pos++] = '.';
    out[pos++] = static_cast<char>('0' + (decimals / 10));
    out[pos++] = static_cast<char>('0' + (decimals % 10));
    out[pos++] = '%';
    out[pos++] = ' ';
    out[pos++] = ' ';
    out[pos++] = ' ';

    sys_io::write_all(STDOUT_FILENO, out, static_cast<size_t>(pos));
}

int run_reversal(const ReversalConfig &config) {
    int ifd = open(config.input_path, O_RDONLY);
    if (ifd < 0) {
        perror("open input");
        return 1;
    }

    struct stat ist;
    if (fstat(ifd, &ist) < 0) {
        perror("fstat input");
        close(ifd);
        return 1;
    }
    if (!S_ISREG(ist.st_mode)) {
        sys_io::write_str(STDERR_FILENO, "Error: input must be a regular file.\n");
        close(ifd);
        return 1;
    }

    off_t size = ist.st_size;
    if (config.mode == ReversalMode::PartialRange &&
        (config.range_start >= size || config.range_end >= size)) {
        sys_io::write_str(STDERR_FILENO, "Error: partial-range indices must fall within the file size.\n");
        close(ifd);
        return 1;
    }

    const char *out_dir = (config.output_dir != nullptr) ? config.output_dir : kDefaultOutputDir;
    if (sys_io::ensure_output_dir(out_dir) < 0) {
        close(ifd);
        return 1;
    }

    const char *name = sys_io::base_name(config.input_path);
    if (name[0] == '\0') {
        sys_io::write_str(STDERR_FILENO, "Error: invalid input filename.\n");
        close(ifd);
        return 1;
    }

    char out_path[kMaxPathSize];
    if (sys_io::build_output_path(out_path, sizeof(out_path),
                                  out_dir, static_cast<int>(config.mode), name) < 0) {
        sys_io::write_str(STDERR_FILENO, "Error: constructed output path exceeds maximum length.\n");
        close(ifd);
        return 1;
    }

    // Create destination with user read/write only (0600); fchmod overrides process umask.
    int ofd = open(out_path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (ofd < 0) {
        perror("open output");
        close(ifd);
        return 1;
    }

    if (fchmod(ofd, 0600) < 0) {
        perror("fchmod output");
        close(ofd);
        close(ifd);
        return 1;
    }

    char *buf = new char[kBufferSize];
    int result = -1;

    switch (config.mode) {
        case ReversalMode::BlockWise:
            result = reverse_block_wise(ifd, ofd, size, config.block_size, buf);
            break;
        case ReversalMode::FullFile:
            result = reverse_full_file(ifd, ofd, size, buf);
            break;
        case ReversalMode::PartialRange:
            result = reverse_partial_range(ifd, ofd, size, config.range_start, config.range_end, buf);
            break;
    }

    if (result == 0 && size == 0) {
        print_progress(0, 0);
    }
    sys_io::write_str(STDOUT_FILENO, "\n");

    delete[] buf;
    close(ofd);
    close(ifd);

    return (result == 0) ? 0 : 1;
}

}  // namespace byteflip
