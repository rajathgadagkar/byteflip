#ifndef BYTEFLIP_COMMON_HPP
#define BYTEFLIP_COMMON_HPP

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <sys/types.h>
#include <stddef.h>

namespace byteflip {

/// Fixed streaming I/O buffer size (1 MiB) to keep memory usage bounded
/// even when processing multi-gigabyte files larger than available RAM.
constexpr size_t kBufferSize = 1024 * 1024;

/// Maximum supported path length for constructed output paths.
constexpr size_t kMaxPathSize = 4096;

/// Default output directory created for transformed files.
constexpr const char *kDefaultOutputDir = "output";

/// Supported file reversal strategies.
enum class ReversalMode : int {
    BlockWise    = 0,  ///< Reverse each fixed-size block independently; preserve block order.
    FullFile     = 1,  ///< Globally reverse the entire file byte-for-byte.
    PartialRange = 2   ///< Reverse [0, start) and (end, EOF); keep [start, end] unchanged.
};

}  // namespace byteflip

#endif  // BYTEFLIP_COMMON_HPP
