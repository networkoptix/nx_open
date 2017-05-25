/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <fstream>
#include "NvVideoEncoder.h"
#include <sstream>
#include <stdint.h>

typedef struct
{
    NvVideoEncoder *enc;
    uint32_t encoder_pixfmt;

    char *in_file_path;
    std::ifstream *in_file;

    uint32_t width;
    uint32_t height;

    char *out_file_path;
    std::ofstream *out_file;

    char *ROI_Param_file_path;
    std::ifstream *roi_Param_file;

    uint32_t bitrate;
    uint32_t profile;
    enum v4l2_mpeg_video_bitrate_mode ratecontrol;
    uint32_t iframe_interval;
    uint32_t idr_interval;
    enum v4l2_mpeg_video_h264_level level;
    uint32_t fps_n;
    uint32_t fps_d;

    enum v4l2_enc_temporal_tradeoff_level_type temporal_tradeoff_level;
    v4l2_enc_slice_length_type slice_length_type;
    uint32_t slice_length;
    uint32_t virtual_buffer_size;
    uint32_t num_reference_frames;
    uint32_t slice_intrarefresh_interval;
    uint32_t num_b_frames;
    uint32_t nMinQpI;              /* Minimum QP value to use for index frames */
    uint32_t nMaxQpI;              /* Maximum QP value to use for index frames */
    uint32_t nMinQpP;              /* Minimum QP value to use for P frames */
    uint32_t nMaxQpP;              /* Maximum QP value to use for P frames */
    uint32_t nMinQpB;              /* Minimum QP value to use for B frames */
    uint32_t nMaxQpB;              /* Maximum QP value to use for B frames */
    bool insert_sps_pps_at_idr;

    bool report_metadata;
    bool dump_mv;

    std::stringstream *runtime_params_str;
    uint32_t next_param_change_frame;
    bool got_error;
} context_t;

int parse_csv_args(context_t * ctx, int argc, char *argv[]);
