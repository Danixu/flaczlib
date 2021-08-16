////////////////////////////////////////////////////////////////////////////////
//
// Created by Daniel Carrasco at https://www.electrosoftcloud.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////////////

#include <climits>
#include <string.h>
#include "FLAC++/encoder.h"
#include "FLAC++/decoder.h"

// Compression flush modes, keeping almost all zlib modes.
// Only two different modes are used:
// * FLACZLIB_NO_FLUSH: Will not flush the data until
enum flaczlib_flush_mode : uint8_t {
    FLACZLIB_NO_FLUSH = 0,
    FLACZLIB_PARTIAL_FLUSH,
    FLACZLIB_SYNC_FLUSH,
    FLACZLIB_FULL_FLUSH,
    FLACZLIB_FINISH,
    FLACZLIB_BLOCK,
};

enum flaczlib_return_code {
    FLACZLIB_RC_OK = 0,
    FLACZLIB_RC_NO_INTIALIZED = INT_MIN,
    FLACZLIB_RC_INITIALIZATION_ERROR,
    FLACZLIB_RC_COMPRESSION_ERROR,
    FLACZLIB_RC_DECOMPRESSION_ERROR,
    FLACZLIB_RC_METADATA_ERROR,
    FLACZLIB_RC_BUFFER_ERROR,
    FLACZLIB_RC_WRONG_OPTIONS,
    FLACZLIB_RC_END_OF_STREAM,
    FLACZLIB_RC_CLOSED
};

#define FLACZLIB_EXTREME_COMPRESSION (uint8_t(1) << 7)

// Stream state similar to zlib state
struct flaczlib_stream {
    uint8_t * next_in;   /* next input byte */
    size_t  avail_in;  /* number of bytes available at next_in */

    uint8_t * next_out;  /* next output byte will go here */
    size_t  avail_out; /* remaining free space at next_out */

    // Decompression buffer
    uint8_t * decompress_buffer_data = NULL;
    size_t decompress_buffer_size = 0;
    size_t decompress_buffer_size_real = 0;
    size_t decompress_buffer_index = 0;

    char *msg;  /* last error message, NULL if no error */
    FLAC__StreamEncoder *encoder_state = NULL;
    FLAC__StreamDecoder *decoder_state = NULL;

    // Compressor status
    flaczlib_return_code status = FLACZLIB_RC_NO_INTIALIZED;
};

class flaczlib {
    public:
        flaczlib(
            bool arg_is_compressor,
            uint8_t arg_channels = 2,
            uint8_t arg_bits_per_sample = 16,
            uint32_t arg_sample_rate = 44100,
            size_t arg_block_size = 0,
            int8_t arg_compression_level = 5
        );
        ~flaczlib();
        int compress(uint32_t samples, flaczlib_flush_mode flush_mode);
        int decompress();
        int decompress_partial(bool reset, long long seek_to = -1);
        flaczlib_return_code get_status() { return flac_strm->status; }
        FLAC__StreamEncoderState get_flac_encoder_status() { return FLAC__stream_encoder_get_state(flac_strm->encoder_state); }
        FLAC__StreamDecoderState get_flac_decoder_status() { return FLAC__stream_decoder_get_state(flac_strm->decoder_state); }
        void close();
        flaczlib_stream * flac_strm;

    private:
        bool is_compressor;
        uint8_t channels;
        uint8_t bits_per_sample;
        uint32_t sample_rate;
        size_t block_size;
        int8_t compression_level;
        bool exhaustive_model_search;
};
