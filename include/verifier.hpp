#ifndef BYTEFLIP_VERIFIER_HPP
#define BYTEFLIP_VERIFIER_HPP

#include "common.hpp"
#include <sys/stat.h>

namespace byteflip {

/// Configuration parameters for verifying a reversed file and its permissions.
struct VerificationConfig {
    const char *new_file_path;
    const char *old_file_path;
    const char *output_dir_path;
    ReversalMode mode;
    off_t block_size;   ///< Used when mode == ReversalMode::BlockWise
    off_t range_start;  ///< Used when mode == ReversalMode::PartialRange
    off_t range_end;    ///< Used when mode == ReversalMode::PartialRange
};

/// Execute streaming content verification and POSIX permission auditing.
/// Prints a structured 30-line verification report to stdout.
/// Returns 0 on successful execution (even if checks report "No"), non-zero on fatal I/O or argument errors.
int run_verification(const VerificationConfig &config);

}  // namespace byteflip

#endif  // BYTEFLIP_VERIFIER_HPP
