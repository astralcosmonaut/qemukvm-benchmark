#include <stdlib.h>
#include <bzlib.h>
#include "bzip2_compression.h"
#include "util.h"

static float mean_compression_time;
static float mean_compression_ratio;
static float mean_decompression_time;

/**
 * @brief Compresses source file to archive file. Measures compression stats. Function saves source data size.
 * @param source source file
 * @param arch archive file
 * @param level compression level
 * @param source_len source data size
 * @return Returns BZIP2_SUCCESS on success or BZIP2_FAILURE if something go wrong.
 */
static int compress(FILE *source, FILE *arch, int level, unsigned int *source_len)
{
    char *buf;
    char *output;
    int buf_size;
    unsigned int output_size;
    int bz_error;
    struct timespec start_ts, stop_ts;

    buf_size = get_file_size(source);
    *source_len = buf_size; // Save input size.

    buf = (char*)malloc(sizeof(char) * buf_size);
    if (!buf) {
        puts("bzip2 compression error: problem with allocating memory for buffer.");
        return BZIP2_FAILURE;
    }

    // To guarantee that the compressed data will fit in its buffer,
    // allocate an output buffer of size 1% larger than the uncompressed data, plus six hundred extra bytes.
    output_size = buf_size + (1/100 * buf_size) + 600;
    output = (char*)malloc(sizeof(char) * output_size);

    if (!output) {
        puts("bzip2 compression error: problem with allocating memory for archive buffer.");
        if (buf) {
            free(buf);
            buf = NULL;
        }

        return BZIP2_FAILURE;
    }

    // Start measure time.
    clock_gettime(CLOCK_REALTIME, &start_ts);

    fread(buf, 1, buf_size, source);

    bz_error = BZ2_bzBuffToBuffCompress(output, &output_size, buf, buf_size, level, 0, 0);

    if (bz_error != BZ_OK) {
        puts("bzip2 error: problems with compression.");
        return BZIP2_FAILURE;
    }

    fwrite(output, 1, output_size, arch);

    // Print/measure stats.
    clock_gettime(CLOCK_REALTIME, &stop_ts);
    struct timespec result_ts = diff(start_ts, stop_ts);
    mean_compression_time += result_ts.tv_nsec / 1000000.0f;
    mean_compression_ratio += (output_size / (float)buf_size) * 100.0f;

    return BZIP2_SUCCESS;
}

/**
 * @brief Decompresses archive file and measures decompression stats.
 * @param arch archive file
 * @param output_file output, decompressed file
 * @param source_len source (uncompressed) data size. It is calculated in compress function.
 * @return Returns BZIP2_SUCCESS on success or BZIP2_FAILURE if something go wrong.
 */
static int decompress(FILE *arch, FILE *output_file, unsigned int source_len)
{
    int bz_error;
    struct timespec start_ts, stop_ts;
    int arch_size = get_file_size(arch);
    char *input = (char*)malloc(sizeof(char) * arch_size);
    char *output = (char*)malloc(sizeof(char) * source_len);

    if (!input) {
        puts("bzip2 error: problem with allocating input buffer.");
        if (output) {
            free(output);
            output = NULL;
            return BZIP2_FAILURE;
        }
    }

    if (!output) {
        puts("bzip2 error: problem with allocating output buffer.");
        if (input) {
            free(input);
            input = NULL;
            return BZIP2_FAILURE;
        }
    }

    // Start measure time.
    clock_gettime(CLOCK_REALTIME, &start_ts);
    fread(input, 1, arch_size, arch);
    bz_error = BZ2_bzBuffToBuffDecompress(output, &source_len, input, arch_size, 0, 0);

    if (bz_error != BZ_OK) {
        puts("bzip2 decompression error: problems with decompression.");
        return BZIP2_FAILURE;
    }

    if (fwrite(output, 1, source_len, output_file) != source_len || ferror(output_file)) {
        puts("bzip2 decompression error: problem with writing to output file");
        return BZIP2_FAILURE;
    }
    clock_gettime(CLOCK_REALTIME, &stop_ts);

    if (input) {
        free(input);
        input = NULL;
    }
    if (output) {
        free(output);
        output = NULL;
    }

    struct timespec result_ts = diff(start_ts, stop_ts);
    mean_decompression_time += result_ts.tv_nsec / 1000000.0f;

    return BZIP2_SUCCESS;
}

int run_bzip2(FILE *source, FILE *arch, FILE *output, int compression_level, int iterations)
{
    unsigned int source_len;
    int level, ret;
    if (compression_level == LOW_COMPRESSION) {
        level = 1;
    } else {
        level = 9;
    }

    printf("bzip2: compression level set on %d\n", level);

    for (int i = 0; i < iterations; ++i) {
        ret = compress(source, arch, level, &source_len);
        if (ret == BZIP2_FAILURE) {
            return ret;
        }

        rewind(source);
        rewind(arch);
    }

    printf("Mean compression ratio: %.2f%%\n", mean_compression_ratio / iterations);
    printf("Mean compression time: %.3f ms\n", mean_compression_time / iterations);

    rewind(arch);
    for (int i = 0; i < iterations; ++i) {
        ret = decompress(arch, output, source_len);
        if (ret == BZIP2_FAILURE) {
            return ret;
        }

        rewind(arch);
    }

    printf("Mean decompression time: %.3f ms\n", mean_decompression_time / iterations);

    return BZIP2_SUCCESS;
}
