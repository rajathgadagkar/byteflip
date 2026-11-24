#ifndef BYTEFLIP_REVERSER_HPP
#define BYTEFLIP_REVERSER_HPP

#include "common.hpp"

namespace byteflip {

/// Configuration parameters for a single file reversal job.
struct ReversalConfig {
    const char *input_path;
    const char *output_dir;
    ReversalMode mode;
    off_t block_size;   ///< Used when mode == ReversalMode::BlockWise
    off_t range_start;  ///< Used when mode == ReversalMode::PartialRange (0-based, inclusive)
    off_t range_end;    ///< Used when mode == ReversalMode::PartialRange (0-based, inclusive)
};

/// Print single-line carriage-return progress percentage ("Written: XX.XX%") to stdout.
void print_progress(off_t written, off_t total);

/// Execute the configured file reversal operation using bounded memory and POSIX syscalls.
/// Returns 0 on success, non-zero on failure.
int run_reversal(const ReversalConfig &config);

}  // namespace byteflip

#endif  // BYTEFLIP_REVERSER_HPP
