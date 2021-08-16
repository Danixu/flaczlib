#include "flaczlib.h"

int main() {
    //setbuf(stdout, NULL);
    printf("Open files\n");
    FILE * in = fopen("/mnt/c/Users/danix/Desktop/test.flac", "rb");
    FILE * out = fopen("/mnt/c/Users/danix/Desktop/test.flac.bin", "wb");

    //printf("Init buffers\n");
    uint8_t in_buffer[0x400000] = {};
    uint8_t out_buffer[2352] = {};
    
    //printf("Initialize the compressor\n");
    flaczlib compressor = flaczlib(false);

    /* Generated flac is an stream, so metadata is at end of file.
     * Must be readed or decoding will fail */

    //fseek(in, sizeof(in_buffer), SEEK_END);
    //fread(in_buffer, sizeof(in_buffer), 1, in);
    //compressor.flac_strm->avail_in = sizeof(in_buffer);
    //compressor.flac_strm->next_in = in_buffer;

    //compressor.get_metadata();

    // Reset the in status
    //compressor.flac_strm->avail_in = 0;
    //fseek(in, 0, SEEK_SET);

    printf("Decompress file\n");
    while (!feof(in) || compressor.flac_strm->avail_in || compressor.get_status() != FLACZLIB_RC_END_OF_STREAM) {
        //printf("Status: %d\n", compressor.get_status());
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
            //printf("Writting...\n");
            fwrite(out_buffer, sizeof(out_buffer) - compressor.flac_strm->avail_out, 1, out);
        }
        else {
            printf("There was an error decompressing the data: %d\n", compressor.get_status());
            break;
        }
    }

    printf("Closing the files\n");

    fflush(out);
    fclose(in);
    fclose(out);
}