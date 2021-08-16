#include "flaczlib.h"

int main() {
    printf("Open files\n");
    FILE * in = fopen("/mnt/c/Users/danix/Desktop/Firebugs (Europe) (En,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi)/Firebugs (Europe) (En,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi) (Track 10).bin", "rb");
    FILE * out = fopen("/mnt/c/Users/danix/Desktop/test.flac", "wb");

    uint8_t in_buffer[2352] = {};
    uint8_t out_buffer[0x400000] = {};

    printf("Initialize the compressor\n");
    flaczlib compressor = flaczlib(true, 2, 16, 44100, 0, 8|FLACZLIB_EXTREME_COMPRESSION);
    compressor.flac_strm->avail_out = sizeof(out_buffer);
    compressor.flac_strm->next_out = out_buffer;

    printf("Compressing the input file\n");
    while (!feof(in)) {
        size_t readed = fread(in_buffer, 1, sizeof(in_buffer), in);

        compressor.flac_strm->avail_in = readed;
        compressor.flac_strm->next_in = in_buffer;

        compressor.compress(readed / 4, FLACZLIB_NO_FLUSH);

        if (compressor.flac_strm->avail_out < (sizeof(out_buffer) * 0.25)) {
            fwrite(out_buffer, sizeof(out_buffer) - compressor.flac_strm->avail_out, 1, out);
            compressor.flac_strm->avail_out = sizeof(out_buffer);
            compressor.flac_strm->next_out = out_buffer;
        }
    }

    compressor.compress(0, FLACZLIB_FINISH);

    if (compressor.flac_strm->avail_out) {
        fwrite(out_buffer, sizeof(out_buffer) - compressor.flac_strm->avail_out, 1, out);
    }

    printf("Finished!!, flushing cache and closing files.\n");

    fflush(out);
    fclose(in);
    fclose(out);
}