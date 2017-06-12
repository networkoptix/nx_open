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

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <linux/v4l2-controls.h>
#include "video_encode.h"

#define CHECK_OPTION_VALUE(argp) if(!*argp || (*argp)[0] == '-') \
                                { \
                                    cerr << "Error: value not specified for option " << arg << endl; \
                                    goto error; \
                                }

#define CSV_PARSE_CHECK_ERROR(condition, str) \
    if (condition) {\
    cerr << "Error: " << str << endl; \
    goto error; \
    }

using namespace std;

static void
print_help(void)
{
    cerr << "\nvideo_encode <in-file> <in-width> <in-height> <encoder-type> <out-file> [OPTIONS]\n\n"
            "Encoder Types:\n"
            "\tH264\n"
            "\tH265\n\n"
            "OPTIONS:\n"
            "\t-h,--help             Prints this text\n"
            "\t--dbg-level <level>   Sets the debug level [Values 0-3]\n\n"
            "\t-br <bitrate>         Bitrate [Default = 4000000]\n"
            "\t-p <profile>          Encoding Profile [Default = baseline]\n"
            "\t-rc <rate-control>    Ratecontrol mode [Default = cbr]\n"
            "\t-ifi <interval>       I-frame Interval [Default = 30]\n"
            "\t-idri <interval>      IDR Interval [Default = 256]\n"
            "\t--insert-spspps-idr   Insert SPS PPS at every IDR [Default = disabled]\n"
            "\t-fps <num> <den>      Encoding fps in num/den [Default = 30/1]\n\n"
            "\t-tt <level>           Temporal Tradeoff level [Default = 0]\n"
            "\t-vbs <size>           Virtual buffer size [Default = 0]\n"
            "\t-nrf <num>            Number of reference frames [Default = 0]\n\n"
            "\t-slt <type>           Slice length type (1 = Number of MBs, 2 = Bytes) [Default = 1]\n"
            "\t-slen <length>        Slice length [Default = 0]\n"
            "\t-sir <interval>       Slice intrarefresh interval [Default = 0]\n\n"
            "\t-nbf <num>            Number of B frames [Default = 0]\n\n"
            "\t-rpc <string>         Change configurable parameters at runtime\n\n"
            "\t--report-metadata     Print encoder output metadata\n"
            "\t--mvdump              Dump encoded motion vectors\n\n"
            "\t-roi <roi_file_path>  Specify roi param file [Default = disabled]\n\n"
            "\t-MinQpI               Specify minimum Qp Value for I frame\n\n"
            "\t-MaxQpI               Specify maximum Qp Value for I frame\n\n"
            "\t-MinQpP               Specify minimum Qp Value for P frame\n\n"
            "\t-MaxQpP               Specify maximum Qp Value for P frame\n\n"
            "\t-MinQpB               Specify minimum Qp Value for B frame\n\n"
            "\t-MaxQpB               Specify maximum Qp Value for B frame\n\n"
            "NOTE: roi parameters need to be feed per frame in following format\n"
            "      <no. of roi regions> <Qpdelta> <left> <top> <width> <height> ...\n"
            "      e.g. [Each line corresponds roi parameters for one frame] \n"
            "      2   -2   34  33  16  19  -3  68  68  16  16\n"
            "      1   -5   40  40  40  40\n"
            "      3   -4   34  34  16  16  -5  70  70  18  18  -3  100  100  34  34\n"
            "Supported Encoding profiles for H.264:\n"
            "\tbaseline\tmain\thigh\n"
            "Supported Encoding profiles for H.265:\n"
            "\tmain\n"
            "Supported Encoding rate control modes:\n"
            "\tcbr\tvbr\n\n"
            "Supported Temporal Tradeoff levels:\n"
            "0:Drop None       1:Drop 1 in 5      2:Drop 1 in 3\n"
            "3:Drop 1 in 2     4:Drop 2 in 3\n\n"
            "Runtime configurable parameter string should be of the form:\n"
            "\"f<frame_num1>,<prop_id1><val>,<prop_id2><val>;f<frame_num1>,<prop_id1><val>,<prop_id2><val>;...\"\n"
            "e.g. \"f20,b8000000,i1;f300,b6000000,r40/1\"\n\n"
            "Property ids:\n"
            "\tb<bitrate>  Bitrate\n"
            "\tr<num/den>  Framerate\n"
            "\ti1          Force I-frame\n\n"
            "NOTE: These encoding parameters are slightly imprecisely updated depending upon the number of\n"
            "frames in queue and/or processed already.\n";
}

static uint32_t
get_encoder_type(char *arg)
{
    if (!strcmp(arg, "H264"))
        return V4L2_PIX_FMT_H264;
    if (!strcmp(arg, "H265"))
        return V4L2_PIX_FMT_H265;
    return 0;
}

static int32_t
get_encoder_ratecontrol(char *arg)
{
    if (!strcmp(arg, "cbr"))
        return V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;

    if (!strcmp(arg, "vbr"))
        return V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;

    return -1;
}

static int32_t
get_encoder_profile_h264(char *arg)
{
    if (!strcmp(arg, "baseline"))
        return V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;

    if (!strcmp(arg, "main"))
        return V4L2_MPEG_VIDEO_H264_PROFILE_MAIN;

    if (!strcmp(arg, "high"))
        return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
    return -1;
}

static int32_t
get_encoder_profile_h265(char *arg)
{
    if (!strcmp(arg, "main"))
        return V4L2_MPEG_VIDEO_H265_PROFILE_MAIN;

    return -1;
}

static int32_t
get_encoder_level(char *arg)
{
    if (!strcmp(arg, "1.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_0;

    if (!strcmp(arg, "1b"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1B;

    if (!strcmp(arg, "1.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_1;

    if (!strcmp(arg, "1.2"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_2;

    if (!strcmp(arg, "1.3"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_1_3;

    if (!strcmp(arg, "2.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_2_0;

    if (!strcmp(arg, "2.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_2_1;

    if (!strcmp(arg, "2.2"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_2_2;

    if (!strcmp(arg, "3.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_3_0;

    if (!strcmp(arg, "3.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_3_1;

    if (!strcmp(arg, "3.2"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_3_2;

    if (!strcmp(arg, "4.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_4_0;

    if (!strcmp(arg, "4.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_4_1;

    if (!strcmp(arg, "4.2"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_4_2;

    if (!strcmp(arg, "5.0"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_5_0;

    if (!strcmp(arg, "5.1"))
        return V4L2_MPEG_VIDEO_H264_LEVEL_5_1;

    return -1;
}

static int32_t
get_dbg_level(char *arg)
{
    int32_t log_level = atoi(arg);

    if (log_level < 0)
    {
        cout << "Warning: invalid log level input, defaulting to setting 0" << endl;
        return 0;
    }

    if (log_level > 3)
    {
        cout << "Warning: invalid log level input, defaulting to setting 3" << endl;
        return 3;
    }

    return log_level;
}

int
parse_csv_args(context_t * ctx, int argc, char *argv[])
{
    char **argp = argv;
    char *arg = *(++argp);
    int32_t intval;

    if (argc == 1 || (arg && (!strcmp(arg, "-h") || !strcmp(arg, "--help"))))
    {
        print_help();
        exit(EXIT_SUCCESS);
    }

    CSV_PARSE_CHECK_ERROR(argc < 6, "Insufficient arguments");

    ctx->in_file_path = strdup(*argp);
    CSV_PARSE_CHECK_ERROR(!ctx->in_file_path, "Input file not specified");

    ctx->width = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->width == 0, "Input width should be > 0");

    ctx->height = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->height == 0, "Input height should be > 0");

    ctx->encoder_pixfmt = get_encoder_type(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->encoder_pixfmt == 0,
                          "Incorrect encoder type");

    ctx->out_file_path = strdup(*(++argp));
    CSV_PARSE_CHECK_ERROR(!ctx->out_file_path, "Output file not specified");

    while ((arg = *(++argp)))
    {
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            print_help();
            exit(EXIT_SUCCESS);
        }
        else if (!strcmp(arg, "--dbg-level"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            log_level = get_dbg_level(*argp);
        }
        else if (!strcmp(arg, "-roi"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->ROI_Param_file_path = strdup(*argp);
        }
        else if (!strcmp(arg, "-fps"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->fps_n = atoi(*argp);
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->fps_d = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->fps_d == 0, "fps den should be > 0");
        }
        else if (!strcmp(arg, "-br"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->bitrate = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->bitrate == 0, "bit rate should be > 0");
        }
        else if (!strcmp(arg, "-ifi"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->iframe_interval = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->iframe_interval == 0,
                    "ifi size shoudl be > 0");
        }
        else if (!strcmp(arg, "-idri"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->idr_interval = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->idr_interval == 0,
                    "idri size shoudl be > 0");
        }
        else if (!strcmp(arg, "--insert-spspps-idr"))
        {
            ctx->insert_sps_pps_at_idr = true;
        }
        else if (!strcmp(arg, "-l"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            intval = get_encoder_level(*argp);
            CSV_PARSE_CHECK_ERROR(intval == -1,
                    "Unsupported value for level: " << *argp);
            ctx->level = (enum v4l2_mpeg_video_h264_level) intval;
        }
        else if (!strcmp(arg, "-rc"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            intval = get_encoder_ratecontrol(*argp);
            CSV_PARSE_CHECK_ERROR(intval == -1,
                    "Unsupported value for ratecontrol: " << *argp);
            ctx->ratecontrol = (enum v4l2_mpeg_video_bitrate_mode) intval;
        }
        else if (!strcmp(arg, "-rpc"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->runtime_params_str = new stringstream(*argp);
        }
        else if (!strcmp(arg, "-p"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            if (ctx->encoder_pixfmt == V4L2_PIX_FMT_H264)
            {
                ctx->profile = get_encoder_profile_h264(*argp);
            }
            else if (ctx->encoder_pixfmt == V4L2_PIX_FMT_H265)
            {
                ctx->profile = get_encoder_profile_h265(*argp);
            }
            CSV_PARSE_CHECK_ERROR(ctx->profile == (uint32_t) -1,
                        "Unsupported value for profile: " << *argp);
        }
        else if (!strcmp(arg, "-tt"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->temporal_tradeoff_level =
                (enum v4l2_enc_temporal_tradeoff_level_type) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->temporal_tradeoff_level <
                     V4L2_ENC_TEMPORAL_TRADEOFF_LEVEL_DROPNONE ||
                     ctx->temporal_tradeoff_level >
                     V4L2_ENC_TEMPORAL_TRADEOFF_LEVEL_DROP2IN3),
                    "Unsupported value for temporal tradeoff: " << *argp);
        }
        else if (!strcmp(arg, "-slt"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            switch (atoi(*argp))
            {
                case 1:
                    ctx->slice_length_type = V4L2_ENC_SLICE_LENGTH_TYPE_MBLK;
                    break;
                case 2:
                    ctx->slice_length_type = V4L2_ENC_SLICE_LENGTH_TYPE_BITS;
                    break;
                default:
                    CSV_PARSE_CHECK_ERROR(true,
                            "Unsupported value for slice length type: " << *argp);
            }
        }
        else if (!strcmp(arg, "-slen"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->slice_length = (uint32_t) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->slice_length == 0,
                    "Slice length should be > 0");
        }
        else if (!strcmp(arg, "-vbs"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->virtual_buffer_size = (uint32_t) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->virtual_buffer_size == 0,
                    "Virtual buffer size should be > 0");
        }
        else if (!strcmp(arg, "-nbf"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->num_b_frames = (uint32_t) atoi(*argp);
        }
        else if (!strcmp(arg, "-nrf"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->num_reference_frames = (uint32_t) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->num_reference_frames == 0,
                    "Num reference frames should be > 0");
        }
        else if (!strcmp(arg, "-sir"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->slice_intrarefresh_interval = (uint32_t) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->slice_intrarefresh_interval == 0,
                    "Slice intrarefresh interval should be > 0");
        }
        else if (!strcmp(arg, "-MinQpI"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nMinQpI = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->nMinQpI < 0, "Min Qp should be >= 0");
        }
        else if (!strcmp(arg, "-MaxQpI"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nMaxQpI = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->nMaxQpI > 51, "Max Qp should be <= 51");
        }
        else if (!strcmp(arg, "-MinQpP"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nMinQpP = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->nMinQpP < 0, "Min Qp should be >= 0");
        }
        else if (!strcmp(arg, "-MaxQpP"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nMaxQpP = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->nMaxQpP > 51, "Max Qp should be <= 51");
        }
        else if (!strcmp(arg, "-MinQpB"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nMinQpB = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->nMinQpB < 0, "Min Qp should be >= 0");
        }
        else if (!strcmp(arg, "-MaxQpB"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->nMaxQpB = atoi(*argp);
            CSV_PARSE_CHECK_ERROR(ctx->nMaxQpB > 51, "Max Qp should be <= 51");
        }
        else if (!strcmp(arg, "--report-metadata"))
        {
            ctx->report_metadata = true;
        }
        else if (!strcmp(arg, "--mvdump"))
        {
            ctx->dump_mv = true;
        }
        else
        {
            CSV_PARSE_CHECK_ERROR(ctx->out_file_path, "Unknown option " << arg);
        }
    }

    return 0;

error:
    print_help();
    return -1;
}
