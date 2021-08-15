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

FLAC__StreamEncoderWriteStatus write_callback(
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

flaczlib::flaczlib(
    bool arg_is_compressor,
    uint8_t arg_channels,
    uint8_t arg_bits_per_sample,
    uint32_t arg_sample_rate,
    size_t arg_block_size,
    int8_t arg_compression_level
){
    // Store the global status varible into a pointer to be used from outside
    flac_strm = &flac_strm_int;
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
        ;
        if ((flac_strm_int.encoder_state = FLAC__stream_encoder_new()) == NULL) {
            printf("There was an error initializing the encoder state\n");
            flac_strm->status = FLACZLIB_RC_INITIALIZATION_ERROR;
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
            flac_strm->status = FLACZLIB_RC_INITIALIZATION_ERROR;
        }
        flac_strm->status = FLACZLIB_RC_OK;
    }
    else {

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
            FLAC__stream_encoder_init_stream(flac_strm_int.encoder_state, write_callback, NULL, NULL, NULL, NULL);
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
                flac_strm->status = FLACZLIB_RC_COMPRESSION_ERROR;
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
        flac_strm->status = FLACZLIB_RC_WRONG_OPTIONS;
        return -1;
    }
    flac_strm->status = FLACZLIB_RC_OK;
    return 0;
}


int flaczlib::decompress(uint32_t samples) {

    return 0;
}

/**
 * @brief Free all reserved resources
 * 
 */
void flaczlib::close() {
    // Free the lz4 state
    if (flac_strm_int.encoder_state) {
        FLAC__stream_encoder_finish(flac_strm_int.encoder_state);
        FLAC__stream_encoder_delete(flac_strm_int.encoder_state);
    }
    
    if (flac_strm_int.decoder_state) {
        FLAC__stream_decoder_finish(flac_strm_int.decoder_state);
        FLAC__stream_decoder_delete(flac_strm_int.decoder_state);
    }

    flac_strm->status = FLACZLIB_RC_NO_CLOSED;
}