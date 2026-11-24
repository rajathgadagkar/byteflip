#define _FILE_OFFSET_BITS 64

#include "verifier.hpp"
#include "sys_io.hpp"

#include <unistd.h>

using namespace byteflip;

namespace {

void print_usage() {
    sys_io::write_str(
        STDERR_FILENO,
        "Usage:\n"
        "  byteflip-verify <new_file> <old_file> <dir> 0 <block_size>\n"
        "  byteflip-verify <new_file> <old_file> <dir> 1\n"
        "  byteflip-verify <new_file> <old_file> <dir> 2 <start> <end>\n");
}

}  // namespace

int main(int argc, char *argv[]) {
    if (argc < 5) {
        print_usage();
        return 1;
    }

    int ok = 0;
    long long mode_ll = sys_io::parse_int64(argv[4], &ok);
    if (!ok || mode_ll < 0 || mode_ll > 2) {
        sys_io::write_str(STDERR_FILENO, "Error: invalid mode flag. Expected 0, 1, or 2.\n");
        return 1;
    }

    VerificationConfig config{};
    config.new_file_path = argv[1];
    config.old_file_path = argv[2];
    config.output_dir_path = argv[3];
    config.mode = static_cast<ReversalMode>(mode_ll);

    if (config.mode == ReversalMode::BlockWise) {
        if (argc != 6) {
            sys_io::write_str(STDERR_FILENO, "Error: mode 0 requires a positive block size.\n");
            return 1;
        }
        long long v = sys_io::parse_int64(argv[5], &ok);
        if (!ok || v <= 0) {
            sys_io::write_str(STDERR_FILENO, "Error: invalid block size.\n");
            return 1;
        }
        config.block_size = static_cast<off_t>(v);
    } else if (config.mode == ReversalMode::FullFile) {
        if (argc != 5) {
            sys_io::write_str(STDERR_FILENO, "Error: mode 1 does not take additional arguments.\n");
            return 1;
        }
    } else {
        if (argc != 7) {
            sys_io::write_str(STDERR_FILENO, "Error: mode 2 requires start and end indices.\n");
            return 1;
        }
        long long s = sys_io::parse_int64(argv[5], &ok);
        if (!ok || s < 0) {
            sys_io::write_str(STDERR_FILENO, "Error: invalid start index.\n");
            return 1;
        }
        long long e = sys_io::parse_int64(argv[6], &ok);
        if (!ok || e < 0) {
            sys_io::write_str(STDERR_FILENO, "Error: invalid end index.\n");
            return 1;
        }
        if (s > e) {
            sys_io::write_str(STDERR_FILENO, "Error: start index cannot exceed end index.\n");
            return 1;
        }
        config.range_start = static_cast<off_t>(s);
        config.range_end = static_cast<off_t>(e);
    }

    return run_verification(config);
}
