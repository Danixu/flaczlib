#include "flaczlib.h"

int main() {
    printf("Open files\n");
    FILE * in = fopen("/mnt/c/Users/danix/Desktop/test_new.iso", "rb");
    FILE * out = fopen("/mnt/c/Users/danix/Desktop/test.flac", "wb");

    uint8_t in_buffer[2352] = {};
    uint8_t out_buffer[0x400000] = {};

    printf("Initialize the compressor\n");
    flaczlib compressor = flaczlib(true, 2, 16, 44100, 0, 8|FLACZLIB_EXTREME_COMPRESSION);
    compressor.strm->avail_out = sizeof(out_buffer);
    compressor.strm->next_out = out_buffer;

    printf("Compressing the input file\n");
    while (!feof(in)) {
        size_t readed = fread(in_buffer, 1, sizeof(in_buffer), in);

        compressor.strm->avail_in = readed;
        compressor.strm->next_in = in_buffer;

        compressor.compress(readed / 4, FLACZLIB_NO_FLUSH);

        if (compressor.strm->avail_out < (sizeof(out_buffer) * 0.25)) {
            fwrite(out_buffer, sizeof(out_buffer) - compressor.strm->avail_out, 1, out);
            compressor.strm->avail_out = sizeof(out_buffer);
            compressor.strm->next_out = out_buffer;
        }
    }

    compressor.compress(0, FLACZLIB_FINISH);

    if (compressor.strm->avail_out) {
        fwrite(out_buffer, sizeof(out_buffer) - compressor.strm->avail_out, 1, out);
    }

    printf("Finished!!, flushing cache and closing files.\n");

    fflush(out);
    fclose(in);
    fclose(out);
}