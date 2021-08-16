#include "flaczlib.h"

int main() {
    //setbuf(stdout, NULL);
    printf("Open files\n");
    FILE * in = fopen("/mnt/c/Users/danix/Desktop/test.flac", "rb");
    FILE * out = fopen("/mnt/c/Users/danix/Desktop/test.flac.bin", "wb");

    //printf("Init buffers\n");
    uint8_t in_buffer[0x400000] = {};
    uint8_t out_buffer[2352] = {};
    
    printf("Initialize the Decompressor\n");
    flaczlib compressor = flaczlib(false);

    printf("Decompressing the input file\n");
    // While in file is not fully readed, there is data in IN buffer and compressor is not in End Of Stream, decompress data.
    while (!feof(in) || compressor.flac_strm->avail_in || compressor.get_status() != FLACZLIB_RC_END_OF_STREAM) {
        if (!feof(in) && compressor.flac_strm->avail_in < (sizeof(in_buffer) * 0.25)) {
            memmove(in_buffer, in_buffer + (sizeof(in_buffer) - compressor.flac_strm->avail_in), compressor.flac_strm->avail_in);

            size_t readed = fread(in_buffer + compressor.flac_strm->avail_in, 1, sizeof(in_buffer) - compressor.flac_strm->avail_in, in);

            compressor.flac_strm->avail_in += readed;
            compressor.flac_strm->next_in = in_buffer;
        }
        
        compressor.flac_strm->avail_out = sizeof(out_buffer);
        compressor.flac_strm->next_out = out_buffer;

        int decompress_status = compressor.decompress_partial(false, -1);

        if (decompress_status == 0) {
            fwrite(out_buffer, sizeof(out_buffer) - compressor.flac_strm->avail_out, 1, out);
        }
        else {
            printf("There was an error decompressing the data: %d\n", compressor.get_status());
            break;
        }
    }

    printf("Finished!!, flushing cache and closing files.\n");

    fflush(out);
    fclose(in);
    fclose(out);
}