#define _FILE_OFFSET_BITS 64

#include "verifier.hpp"
#include "sys_io.hpp"

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

namespace byteflip {

namespace {

void print_yes_no(int value) {
    sys_io::write_str(STDOUT_FILENO, value ? "Yes" : "No");
}

/// Compare two equally sized byte ranges directly (or reversed within each chunk).
int check_direct_range(int fd_old, int fd_new, off_t old_pos, off_t new_pos,
                       off_t length, char *old_buf, char *new_buf) {
    off_t done = 0;

    while (done < length) {
        off_t remain = length - done;
        size_t want = (remain > static_cast<off_t>(kBufferSize))
                          ? kBufferSize
                          : static_cast<size_t>(remain);

        if (lseek(fd_old, old_pos + done, SEEK_SET) == static_cast<off_t>(-1)) return 0;
        if (lseek(fd_new, new_pos + done, SEEK_SET) == static_cast<off_t>(-1)) return 0;

        size_t got_old = 0;
        size_t got_new = 0;
        if (sys_io::read_full(fd_old, old_buf, want, &got_old) < 0 || got_old != want) return 0;
        if (sys_io::read_full(fd_new, new_buf, want, &got_new) < 0 || got_new != want) return 0;

        for (size_t i = 0; i < want; ++i) {
            if (old_buf[i] != new_buf[i]) return 0;
        }
        done += static_cast<off_t>(want);
    }
    return 1;
}

/// Verify that [new_start, new_end) in `new_fd` is the exact byte-for-byte reverse
/// of [old_start, old_end) in `old_fd`, streaming in bounded chunks.
int check_reversed_range(int old_fd, int new_fd,
                         off_t old_start, off_t old_end,
                         off_t new_start, off_t new_end,
                         char *old_buf, char *new_buf) {
    off_t length = old_end - old_start;
    if (length != new_end - new_start) return 0;

    off_t done = 0;
    while (done < length) {
        off_t remain = length - done;
        size_t want = (remain > static_cast<off_t>(kBufferSize))
                          ? kBufferSize
                          : static_cast<size_t>(remain);

        off_t old_pos = old_start + done;
        off_t new_pos = new_end - done - static_cast<off_t>(want);

        if (lseek(old_fd, old_pos, SEEK_SET) == static_cast<off_t>(-1)) return 0;
        if (lseek(new_fd, new_pos, SEEK_SET) == static_cast<off_t>(-1)) return 0;

        size_t got_old = 0;
        size_t got_new = 0;
        if (sys_io::read_full(old_fd, old_buf, want, &got_old) < 0 || got_old != want) return 0;
        if (sys_io::read_full(new_fd, new_buf, want, &got_new) < 0 || got_new != want) return 0;

        for (size_t i = 0; i < want; ++i) {
            if (old_buf[i] != new_buf[want - 1 - i]) return 0;
        }
        done += static_cast<off_t>(want);
    }
    return 1;
}

int verify_block_wise(int old_fd, int new_fd, off_t size, off_t block_size,
                      char *old_buf, char *new_buf) {
    off_t pos = 0;
    while (pos < size) {
        off_t remain = size - pos;
        off_t current_block = (remain < block_size) ? remain : block_size;
        if (!check_reversed_range(old_fd, new_fd,
                                  pos, pos + current_block,
                                  pos, pos + current_block,
                                  old_buf, new_buf)) {
            return 0;
        }
        pos += current_block;
    }
    return 1;
}

int verify_full_file(int old_fd, int new_fd, off_t size,
                     char *old_buf, char *new_buf) {
    return check_reversed_range(old_fd, new_fd, 0, size, 0, size, old_buf, new_buf);
}

int verify_partial_range(int old_fd, int new_fd, off_t size,
                         off_t start, off_t end,
                         char *old_buf, char *new_buf) {
    if (start > 0 &&
        !check_reversed_range(old_fd, new_fd, 0, start, 0, start, old_buf, new_buf)) {
        return 0;
    }

    off_t middle_len = end - start + 1;
    if (middle_len > 0 &&
        !check_direct_range(old_fd, new_fd, start, start, middle_len, old_buf, new_buf)) {
        return 0;
    }

    off_t suffix_start = end + 1;
    if (suffix_start < size &&
        !check_reversed_range(old_fd, new_fd,
                              suffix_start, size,
                              suffix_start, size,
                              old_buf, new_buf)) {
        return 0;
    }

    return 1;
}

/// Print all 9 POSIX permission bits (User/Group/Others x Read/Write/Execute) for a target.
void print_permissions(const char *target_label, mode_t mode) {
    struct PermCheck {
        const char *prefix;
        mode_t mask;
    };

    const PermCheck checks[9] = {
        {"User has read permissions on ",     S_IRUSR},
        {"User has write permission on ",     S_IWUSR},
        {"User has execute permission on ",   S_IXUSR},
        {"Group has read permissions on ",    S_IRGRP},
        {"Group has write permission on ",    S_IWGRP},
        {"Group has execute permission on ",  S_IXGRP},
        {"Others has read permissions on ",   S_IROTH},
        {"Others has write permission on ",   S_IWOTH},
        {"Others has execute permission on ", S_IXOTH}
    };

    for (int i = 0; i < 9; ++i) {
        sys_io::write_str(STDOUT_FILENO, checks[i].prefix);
        sys_io::write_str(STDOUT_FILENO, target_label);
        sys_io::write_str(STDOUT_FILENO, ": ");
        print_yes_no((mode & checks[i].mask) != 0);
        sys_io::write_str(STDOUT_FILENO, "\n");
    }
}

}  // namespace

int run_verification(const VerificationConfig &config) {
    int new_fd = open(config.new_file_path, O_RDONLY);
    if (new_fd < 0) {
        perror("open new file");
        return 1;
    }

    int old_fd = open(config.old_file_path, O_RDONLY);
    if (old_fd < 0) {
        perror("open old file");
        close(new_fd);
        return 1;
    }

    struct stat new_st{};
    struct stat old_st{};
    struct stat dir_st{};

    if (fstat(new_fd, &new_st) < 0) {
        perror("fstat new file");
        close(new_fd);
        close(old_fd);
        return 1;
    }
    if (fstat(old_fd, &old_st) < 0) {
        perror("fstat old file");
        close(new_fd);
        close(old_fd);
        return 1;
    }
    if (stat(config.output_dir_path, &dir_st) < 0) {
        perror("stat directory");
        close(new_fd);
        close(old_fd);
        return 1;
    }

    int directory_exists = S_ISDIR(dir_st.st_mode) ? 1 : 0;
    int same_size = (new_st.st_size == old_st.st_size) ? 1 : 0;
    int contents_ok = 0;

    char *old_buf = new char[kBufferSize];
    char *new_buf = new char[kBufferSize];

    if (directory_exists && same_size) {
        if (config.mode == ReversalMode::PartialRange &&
            (config.range_start >= old_st.st_size || config.range_end >= old_st.st_size)) {
            contents_ok = 0;
        } else {
            switch (config.mode) {
                case ReversalMode::BlockWise:
                    contents_ok = verify_block_wise(old_fd, new_fd, old_st.st_size,
                                                    config.block_size, old_buf, new_buf);
                    break;
                case ReversalMode::FullFile:
                    contents_ok = verify_full_file(old_fd, new_fd, old_st.st_size,
                                                   old_buf, new_buf);
                    break;
                case ReversalMode::PartialRange:
                    contents_ok = verify_partial_range(old_fd, new_fd, old_st.st_size,
                                                       config.range_start, config.range_end,
                                                       old_buf, new_buf);
                    break;
            }
        }
    }

    sys_io::write_str(STDOUT_FILENO, "Directory is created: ");
    print_yes_no(directory_exists);
    sys_io::write_str(STDOUT_FILENO, "\n");

    sys_io::write_str(STDOUT_FILENO, "Whether file contents are correctly processed: ");
    print_yes_no(contents_ok);
    sys_io::write_str(STDOUT_FILENO, "\n");

    sys_io::write_str(STDOUT_FILENO, "Both Files Sizes are Same: ");
    print_yes_no(same_size);
    sys_io::write_str(STDOUT_FILENO, "\n");

    print_permissions("newfile", new_st.st_mode);
    print_permissions("oldfile", old_st.st_mode);
    print_permissions("directory", dir_st.st_mode);

    delete[] old_buf;
    delete[] new_buf;
    close(new_fd);
    close(old_fd);

    return 0;
}

}  // namespace byteflip
