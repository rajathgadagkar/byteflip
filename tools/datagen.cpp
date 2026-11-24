#define _FILE_OFFSET_BITS 64

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

namespace {

constexpr size_t kChunkSize = 100000;
constexpr char kAlphabet[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

void fill_random_chunk(char *buf, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        buf[i] = kAlphabet[rand() % 62];
    }
}

int write_chunk(int fd, const char *buf, size_t count) {
    size_t done = 0;
    while (done < count) {
        ssize_t n = write(fd, buf + done, count - done);
        if (n <= 0) return -1;
        done += static_cast<size_t>(n);
    }
    return 0;
}

}  // namespace

int main(int argc, char *argv[]) {
    const char *filename = (argc >= 2) ? argv[1] : "sample.bin";
    long long total_bytes = (argc >= 3) ? strtoll(argv[2], nullptr, 10) : 1000000LL;

    if (total_bytes < 0) {
        fprintf(stderr, "Error: file size must be non-negative.\n");
        return 1;
    }

    srand(69420);

    int ofd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (ofd < 0) {
        perror("open");
        return 1;
    }

    char *buf = static_cast<char *>(malloc(kChunkSize));
    if (buf == nullptr) {
        fprintf(stderr, "Error: failed to allocate buffer.\n");
        close(ofd);
        return 1;
    }

    struct timeval start{}, curr{};
    gettimeofday(&start, nullptr);

    long long written = 0;
    long long chunks = total_bytes / static_cast<long long>(kChunkSize);
    size_t rem = static_cast<size_t>(total_bytes % static_cast<long long>(kChunkSize));

    printf("Generating %s (%lld bytes)...\n", filename, total_bytes);

    for (long long i = 0; i < chunks; ++i) {
        fill_random_chunk(buf, kChunkSize);
        if (write_chunk(ofd, buf, kChunkSize) < 0) {
            perror("write");
            free(buf);
            close(ofd);
            return 1;
        }
        written += static_cast<long long>(kChunkSize);

        if (i % 500 == 0 && total_bytes > 0) {
            gettimeofday(&curr, nullptr);
            double elapsed = (curr.tv_sec - start.tv_sec) +
                             (curr.tv_usec - start.tv_usec) / 1000000.0;
            double percent = (static_cast<double>(written) / static_cast<double>(total_bytes)) * 100.0;
            double speed = (elapsed > 0.0) ? ((written / 1024.0 / 1024.0) / elapsed) : 0.0;

            int bars = static_cast<int>(percent / 2.0);
            printf("\r[");
            for (int j = 0; j < 50; ++j) printf(j < bars ? "=" : " ");
            printf("] %.1f%% %.1f MB/s", percent, speed);
            fflush(stdout);
        }
    }

    if (rem > 0) {
        fill_random_chunk(buf, rem);
        if (write_chunk(ofd, buf, rem) < 0) {
            perror("write");
            free(buf);
            close(ofd);
            return 1;
        }
        written += static_cast<long long>(rem);
    }

    gettimeofday(&curr, nullptr);
    double total_time = (curr.tv_sec - start.tv_sec) +
                        (curr.tv_usec - start.tv_usec) / 1000000.0;

    printf("\r[==================================================] 100.0%%\n");
    printf("Done: wrote %lld bytes in %.2f s\n", written, total_time);

    free(buf);
    close(ofd);
    return 0;
}
