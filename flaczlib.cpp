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

#include "flaczlib.h"
#include <stdlib.h>
#include <string.h>
#include <iostream>

// Global stream status
flaczlib_stream flac_strm_int;

static unsigned round_to_bytes(unsigned bits)
{
  return (bits + (bits % 8)) / 8;
}


FLAC__StreamEncoderWriteStatus encode_write_callback(
    const FLAC__StreamEncoder *encoder,
    const FLAC__byte buffer[],
    size_t bytes,
    unsigned samples,
    unsigned current_frame,
    void *client_data
) {
    //printf("Writting %d bytes - %d samples - current_frame %d\n", bytes, samples, current_frame);
    if (bytes > 0) {
        if (bytes > flac_strm_int.avail_out) {
            return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
        }
        else {
            //printf("memcpy\n");
            memcpy(flac_strm_int.next_out, buffer, bytes);
            flac_strm_int.next_out += bytes;
            flac_strm_int.avail_out -= bytes;
            return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
        }
    }
    else {
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
    }

    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}


FLAC__StreamDecoderReadStatus decode_read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte *buffer, size_t *bytes, void *client_data
) {
    if (*bytes > 0) {
        size_t to_read = *bytes;
        if (*bytes > flac_strm_int.avail_in) {
            to_read = flac_strm_int.avail_in;
        }

        if (!to_read) {
            flac_strm_int.status = FLACZLIB_RC_END_OF_STREAM;
            *bytes = 0;
            return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
        }

        memcpy(buffer, flac_strm_int.next_in, to_read);
        flac_strm_int.next_in += to_read;
        flac_strm_int.avail_in -= to_read;
        *bytes = to_read;

        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }
    else {
        *bytes = 0;
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}


::FLAC__StreamDecoderWriteStatus decode_write_callback(
    const FLAC__StreamDecoder *decoder,
    const FLAC__Frame *frame,
    const FLAC__int32 *const ibuffer[],
    void *client_data
) {
    FLAC__uint64 i, ch;
    unsigned channels = frame->header.channels;

    (void)decoder;

    size_t block_size_bytes = 0;
    switch (round_to_bytes(frame->header.bits_per_sample)) {
        case 1:
            block_size_bytes = frame->header.blocksize * channels;
            break;

        case 2:
            block_size_bytes = frame->header.blocksize * channels * 2;
            break;

        case 3:
        case 4:
            // For both cases int32 will be used
            block_size_bytes = frame->header.blocksize * channels * 4;
            break;

    }

    if (block_size_bytes == 0) {
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    if (block_size_bytes > flac_strm_int.decompress_buffer_size_real) {
        // Free the old buffer if exists
        if (flac_strm_int.decompress_buffer_data) {
            free(flac_strm_int.decompress_buffer_data);
        }
        // And create a new one
        flac_strm_int.decompress_buffer_data = (uint8_t*) malloc(block_size_bytes);

        // If there was any error, then return an error
        if (!flac_strm_int.decompress_buffer_data) {
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }

        flac_strm_int.decompress_buffer_size_real = block_size_bytes;
    }

    flac_strm_int.decompress_buffer_index = 0;
    flac_strm_int.decompress_buffer_size = 0;

    switch (round_to_bytes(frame->header.bits_per_sample)) {
        case 1:
            /* If sample width is equal or less than 8 bit, we deal with one
             * byte per sample and samples are unsigned as per WAVE spec. */
            for (i = 0; i < frame->header.blocksize; i++) {
                for (ch = 0; ch < channels; ch++) {
                    /* Shift back into unsigned values by adding 0x7f. */
                    *((FLAC__uint8 *)(flac_strm_int.decompress_buffer_data + flac_strm_int.decompress_buffer_size)) = (FLAC__uint8)ibuffer[ch][i] + 0x80;
                    flac_strm_int.decompress_buffer_size += sizeof(FLAC__uint8);
                }
            }
            break;

        case 2:
            // Here we have signed samples, each having width equal to 16 bits
            for (i = 0; i < frame->header.blocksize; i++) {
                for (ch = 0; ch < channels; ch++) {
                    *((FLAC__int16 *)(flac_strm_int.decompress_buffer_data + flac_strm_int.decompress_buffer_size)) = (FLAC__int16)ibuffer[ch][i];
                    flac_strm_int.decompress_buffer_size += sizeof(FLAC__int16);
                }
            }
            break;

        case 3:
        case 4:
            // Singed 24/32 bit samples. Going with 3 bytes step is not so handy so 4 bytes will be used in both cases
            for (i = 0; i < frame->header.blocksize; i++) {
                for (ch = 0; ch < channels; ch++) {
                    *(FLAC__int32 *)((FLAC__byte *)(flac_strm_int.decompress_buffer_data + flac_strm_int.decompress_buffer_size)) = (FLAC__int32)ibuffer[ch][i];
                    flac_strm_int.decompress_buffer_size += sizeof(FLAC__int32);
                }
            }
            break;
    }

  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


void decode_error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) {
    printf("There was an error decoding the stream: %d\n", status);

    // Change the state only if not in EOF
    flac_strm_int.status = FLACZLIB_RC_DECOMPRESSION_ERROR;
}


void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data) {
	/* print some stats */
	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
		fprintf(stderr, "sample rate    : %u Hz\n", metadata->data.stream_info.sample_rate);
		fprintf(stderr, "channels       : %u\n", metadata->data.stream_info.channels);
		fprintf(stderr, "bits per sample: %u\n", metadata->data.stream_info.bits_per_sample);
		fprintf(stderr, "total samples  : %d\n", metadata->data.stream_info.total_samples);
	}
}


flaczlib::flaczlib(
    bool arg_is_compressor,
    uint8_t arg_channels,
    uint8_t arg_bits_per_sample,
    uint32_t arg_sample_rate,
    size_t arg_block_size,
    int8_t arg_compression_level
){
    // Store the global status varible into a pointer to be used from outside
    strm = &flac_strm_int;
    // Input data
    flac_strm_int.next_in = NULL;
    flac_strm_int.avail_in = 0;

    // Output data
    flac_strm_int.next_out = NULL;
    flac_strm_int.avail_out = 0;

    // Store the initializing options
    is_compressor = arg_is_compressor;
    channels = arg_channels;
    bits_per_sample = arg_bits_per_sample;
    sample_rate = arg_sample_rate;
    block_size = arg_block_size;
    if (arg_compression_level & FLACZLIB_EXTREME_COMPRESSION) {
        // Extreme compression
        compression_level = arg_compression_level & ~FLACZLIB_EXTREME_COMPRESSION;
        exhaustive_model_search = true;
    }
    else {
        compression_level = arg_compression_level;
        exhaustive_model_search = false;
    }

    if (is_compressor) {
        // Create the new encoder
        if ((flac_strm_int.encoder_state = FLAC__stream_encoder_new()) == NULL) {
            printf("There was an error initializing the encoder state\n");
            flac_strm_int.status = FLACZLIB_RC_INITIALIZATION_ERROR;
        }
        if (
            // Configure the encoder settings
            !FLAC__stream_encoder_set_verify(flac_strm_int.encoder_state, false) ||
            !FLAC__stream_encoder_set_streamable_subset(flac_strm_int.encoder_state, true) ||
            !FLAC__stream_encoder_set_channels(flac_strm_int.encoder_state, channels) ||
            !FLAC__stream_encoder_set_bits_per_sample(flac_strm_int.encoder_state, bits_per_sample) ||
            !FLAC__stream_encoder_set_sample_rate(flac_strm_int.encoder_state, sample_rate) ||
            !FLAC__stream_encoder_set_blocksize(flac_strm_int.encoder_state, block_size) ||
            !FLAC__stream_encoder_set_compression_level(flac_strm_int.encoder_state, compression_level) ||
            !FLAC__stream_encoder_set_do_exhaustive_model_search(flac_strm_int.encoder_state, exhaustive_model_search)
        ) {
            printf("There was an error creating the encoder\n");
            flac_strm_int.status = FLACZLIB_RC_INITIALIZATION_ERROR;
        }
        flac_strm_int.status = FLACZLIB_RC_OK;
    }
    else {
        if ((flac_strm_int.decoder_state = FLAC__stream_decoder_new()) == NULL) {
            printf("There was an error initializing the decoder state\n");
            flac_strm_int.status = FLACZLIB_RC_INITIALIZATION_ERROR;
        }
        if (
            // Configure the decoder settings
            !FLAC__stream_decoder_set_md5_checking(flac_strm_int.decoder_state, false)
        ) {

        }
    }
}

flaczlib::~flaczlib() {
    close();
}

int flaczlib::compress(uint32_t samples, flaczlib_flush_mode flush_mode) {
    if (is_compressor) {
        // Stream cannot be initialized at class constructor because it needs at least 4 bytes in buffer.
        // That is why I am initializing the encoder if it is not already initialized
        if (FLAC__stream_encoder_get_state(flac_strm_int.encoder_state) == FLAC__STREAM_ENCODER_UNINITIALIZED) {
            FLAC__stream_encoder_init_stream(flac_strm_int.encoder_state, encode_write_callback, NULL, NULL, NULL, NULL);
        }

        if (samples) {
            // Initialize the buffer
            FLAC__int32 pcm32[samples * channels] = {};

            for (int i = 0; i < (samples * 2); i++) {
                if (bits_per_sample == 8) {
                    // Keep the same bits
                    int8_t input8 = *(int8_t*)(flac_strm_int.next_in + (i * channels));
                    pcm32[i] = input8;
                }
                else if (bits_per_sample == 16) {
                    // Tricky conversion from uint8 to int16
                    int16_t input16 = *(int16_t*)(flac_strm_int.next_in + (i * channels));
                    pcm32[i] = input16;
                }
                else if (bits_per_sample == 32) {
                    // Tricky conversion from uint8 to int32
                    int32_t input32 = *(int32_t*)(flac_strm_int.next_in + (i * channels));
                    pcm32[i] = input32;
                }
                
            }

            FLAC__bool status = FLAC__stream_encoder_process_interleaved(flac_strm_int.encoder_state, pcm32, samples);
            if (!status) {
                fprintf(stderr, "ERROR: processing WAVE file: %s\n", FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(flac_strm_int.encoder_state)]);
                flac_strm_int.status = FLACZLIB_RC_COMPRESSION_ERROR;
                return -1;
            }
            flac_strm_int.next_in += (samples * (bits_per_sample / 8) * channels);
            flac_strm_int.avail_in -= (samples * (bits_per_sample / 8) * channels);
        }
        if (flush_mode == FLACZLIB_FINISH) {
            FLAC__stream_encoder_finish(flac_strm_int.encoder_state);
        }
    }
    else {
        flac_strm_int.status = FLACZLIB_RC_WRONG_OPTIONS;
        return -1;
    }
    flac_strm_int.status = FLACZLIB_RC_OK;
    return 0;
}


int flaczlib::decompress() {
    if (!is_compressor) {
        // Stream cannot be initialized at class constructor because it needs at least 4 bytes in buffer.
        // That is why I am initializing the decoder if it is not already initialized
        if (FLAC__stream_decoder_get_state(flac_strm_int.decoder_state) == FLAC__STREAM_DECODER_UNINITIALIZED) {
            FLAC__StreamDecoderInitStatus init_status = FLAC__stream_decoder_init_stream(flac_strm_int.decoder_state, decode_read_callback, NULL, NULL, NULL, NULL, decode_write_callback, metadata_callback, decode_error_callback, NULL);

            if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
                printf("Initialization error: %d\n", init_status);
                flac_strm_int.status = FLACZLIB_RC_INITIALIZATION_ERROR;
            }
        }

        while (flac_strm_int.avail_in) {
            if (!FLAC__stream_decoder_process_single(flac_strm_int.decoder_state)) {
                return FLACZLIB_RC_DECOMPRESSION_ERROR;
            }

            if (flac_strm_int.decompress_buffer_size > flac_strm_int.avail_out) {
                return FLACZLIB_RC_BUFFER_ERROR;
            }

            memcpy(flac_strm_int.next_out, flac_strm_int.decompress_buffer_data, flac_strm_int.decompress_buffer_size);

            flac_strm_int.next_out += flac_strm_int.decompress_buffer_size;
            flac_strm_int.avail_out -= flac_strm_int.decompress_buffer_size;

            if (flac_strm_int.avail_out == 0) {
                // There's no more space in output buffer so exit the loop
                break;
            }
        }
    }
    else {
        flac_strm_int.status = FLACZLIB_RC_WRONG_OPTIONS;
        return -1;
    }
    return 0;
}


int flaczlib::decompress_partial(bool reset, long long seek_to) {
    if (!is_compressor) {
        if (FLAC__stream_decoder_get_state(flac_strm_int.decoder_state) == FLAC__STREAM_DECODER_UNINITIALIZED) {
            FLAC__StreamDecoderInitStatus init_status = FLAC__stream_decoder_init_stream(flac_strm_int.decoder_state, decode_read_callback, NULL, NULL, NULL, NULL, decode_write_callback, NULL, decode_error_callback, NULL);

            if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
                flac_strm_int.status = FLACZLIB_RC_INITIALIZATION_ERROR;
                printf("Initialization error: %d\n", init_status);
                return -1;
            }
            else {
                flac_strm_int.status = FLACZLIB_RC_OK;
            }
        }

        while (flac_strm_int.avail_out) {
            // If there is no more data in buffer or reset == true, read more data
            if (!(flac_strm_int.decompress_buffer_size - flac_strm_int.decompress_buffer_index) || reset) {
                if (!FLAC__stream_decoder_process_single(flac_strm_int.decoder_state)) {
                    printf("Decompression error\n");
                    flac_strm_int.status = FLACZLIB_RC_DECOMPRESSION_ERROR;
                    return -1;
                }

                if (get_flac_decoder_status() == FLAC__STREAM_DECODER_END_OF_STREAM) {
                    return 0;
                }
            }
            if (seek_to != -1) {
                flac_strm_int.decompress_buffer_index = seek_to;
                seek_to = -1; // We will set the seek_to to -1 to avoid to reset the pointer all the time
            }

            // Copy buffer data to output buffer
            size_t to_read = std::min(flac_strm_int.decompress_buffer_size - flac_strm_int.decompress_buffer_index, flac_strm_int.avail_out);
            memcpy(flac_strm_int.next_out, flac_strm_int.decompress_buffer_data + flac_strm_int.decompress_buffer_index, to_read);

            flac_strm_int.decompress_buffer_index += to_read;
            flac_strm_int.avail_out -= to_read;
            flac_strm_int.next_out += to_read;
        }
    }
    else {
        flac_strm_int.status = FLACZLIB_RC_WRONG_OPTIONS;
        return -1;
    }

    return 0;
}

/**
 * @brief Free all reserved resources
 * 
 */
void flaczlib::close() {
    // Free the flac states
    if (flac_strm_int.encoder_state) {
        FLAC__stream_encoder_finish(flac_strm_int.encoder_state);
        FLAC__stream_encoder_delete(flac_strm_int.encoder_state);
        flac_strm_int.encoder_state = NULL;
    }
    
    if (flac_strm_int.decoder_state) {
        FLAC__stream_decoder_finish(flac_strm_int.decoder_state); // Must be fixed, because is causing segmentation fault
        FLAC__stream_decoder_delete(flac_strm_int.decoder_state);
        flac_strm_int.decoder_state = NULL;
    }

    // Free the buffer if exists
    if (flac_strm_int.decompress_buffer_data) {
        free(flac_strm_int.decompress_buffer_data);
        flac_strm_int.decompress_buffer_size = 0;
        flac_strm_int.decompress_buffer_size_real = 0;
        flac_strm_int.decompress_buffer_index = 0;
        flac_strm_int.decompress_buffer_data = NULL;
    }

    flac_strm_int.status = FLACZLIB_RC_CLOSED;
}