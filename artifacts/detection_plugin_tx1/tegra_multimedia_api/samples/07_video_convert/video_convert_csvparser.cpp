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

#include "video_convert.h"

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
    cerr <<
        "\nvideo-convert <in-file> <in-width> <in-height> <in-format> <out-file> <out-width> <out-height> <out-format> [OPTIONS]\n\n"
        "Supported formats:\n"
        "\tYUV420M\n"
        "\tYVU420M\n"
        "\tNV12M\n"
        "\tYUV444M\n"
        "\tYUV422M\n"
        "\tYUYV\n"
        "\tYVYU\n"
        "\tUYVY\n"
        "\tVYUY\n"
        "\tABGR32\n"
        "\tXRGB32\n"
        "\tGREY\n\n"
        "OPTIONS:\n"
        "\t-h,--help            Prints this text\n"
        "\t--dbg-level <level>  Sets the debug level [Values 0-3]\n\n"
        "\t--input-raw          Input will be raw buffer\n"
        "\t--input-nvpl         Input will be NV PL buffer [Default]\n"
        "\t--input-nvbl         Input will be NV BL buffer\n"
        "\t--output-raw         Output will be raw buffer\n"
        "\t--output-nvpl        Output will be NV PL buffer [Default]\n"
        "\t--output-nvbl        Output will be NV BL buffer\n\n"
        "\t-cr <left> <top> <width> <height> Set the cropping rectangle [Default = 0 0 0 0]\n"
        "\t-fm <method>         Flip method to use [Default = 0]\n"
        "\t-im <method>         Interpolation method to use [Default = 1]\n"
        "\t-tnr <algo>          TNR algorithm to use [Default = 0]\n\n"
        "NOTE: TNR is supported for YUV420M, NV12M, YUYV and UYVY formats\n\n"
        "Allowed values for flip method:\n"
        "0 = Identity(no rotation)  1 = 90 degree counter-clockwise rotation\n"
        "2 = 180 degree rotation    3 = 90 degree clockwise rotation\n"
        "4 = Horizontal flip        5 = Flip across upper right/lower left diagonal\n"
        "6 = Vertical flip          7 = Flip across upper left/lower right diagonal\n\n"
        "Allowed values for interpolation method:\n"
        "1 = nearest    2 = linear\n"
        "3 = smart      4 = bilinear\n\n"
        "Allowed values for tnr:\n"
        "0 = Original               1 = Outdoor low light\n"
        "2 = Outdoor medium light   3 = Outdoor high light\n"
        "4 = Indoor low light       5 = Indoor medium light\n"
        "6 = Indoor high light\n\n";
}

static uint32_t
get_pixelformat(char *arg)
{
    if (!strcmp(arg, "YUV420M"))
        return V4L2_PIX_FMT_YUV420M;

    if (!strcmp(arg, "YVU420M"))
        return V4L2_PIX_FMT_YVU420M;

    if (!strcmp(arg, "NV12M"))
        return V4L2_PIX_FMT_NV12M;

    if (!strcmp(arg, "YUV444M"))
        return V4L2_PIX_FMT_YUV444M;

    if (!strcmp(arg, "YUV422M"))
        return V4L2_PIX_FMT_YUV422M;

    if (!strcmp(arg, "YUYV"))
        return V4L2_PIX_FMT_YUYV;

    if (!strcmp(arg, "YVYU"))
        return V4L2_PIX_FMT_YVYU;

    if (!strcmp(arg, "UYVY"))
        return V4L2_PIX_FMT_UYVY;

    if (!strcmp(arg, "VYUY"))
        return V4L2_PIX_FMT_VYUY;

    if (!strcmp(arg, "ABGR32"))
        return V4L2_PIX_FMT_ABGR32;

    if (!strcmp(arg, "XRGB32"))
        return V4L2_PIX_FMT_XRGB32;

    if (!strcmp(arg, "GREY"))
        return V4L2_PIX_FMT_GREY;

    return 0;
}

static int32_t
get_dbg_level(char *arg)
{
    int32_t log_level = atoi(arg);

    if (log_level < 0)
    {
        cout<<"log level too small, set to 0"<<endl;
        return 0;
    }

    if (log_level > 3)
    {
        cout<<"log level too high, set to 3"<<endl;
        return 3;
    }

    return log_level;
}

int
parse_csv_args(context_t * ctx, int argc, char *argv[])
{
    char **argp = argv;
    char *arg = *(++argp);

    if (argc == 1 || (arg && (!strcmp(arg, "-h") || !strcmp(arg, "--help"))))
    {
        print_help();
        exit(EXIT_SUCCESS);
    }

    CSV_PARSE_CHECK_ERROR(argc < 9, "Insufficient arguments");

    ctx->in_file_path = strdup(*argp);
    CSV_PARSE_CHECK_ERROR(!ctx->in_file_path, "Input file not specified");

    ctx->in_width = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->in_width == 0, "Input width should be > 0");

    ctx->in_height = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->in_height == 0, "Input height should be > 0");

    ctx->in_pixfmt = get_pixelformat(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->in_pixfmt == 0, "Incorrect input format");

    ctx->out_file_path = strdup(*(++argp));
    CSV_PARSE_CHECK_ERROR(!ctx->out_file_path, "Output file not specified");

    ctx->out_width = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->out_width == 0, "Output width should be > 0");

    ctx->out_height = atoi(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->out_height == 0, "Output height should be > 0");

    ctx->out_pixfmt = get_pixelformat(*(++argp));
    CSV_PARSE_CHECK_ERROR(ctx->out_pixfmt == 0, "Incorrect output format");

    while ((arg = *(++argp)))
    {
        if (!strcmp(arg, "--dbg-level"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            log_level = get_dbg_level(*argp);
        }
        else if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            print_help();
            exit(EXIT_SUCCESS);
        }
        else if (!strcmp(arg, "--input-raw"))
        {
            ctx->in_buftype = BUF_TYPE_RAW;
        }
        else if (!strcmp(arg, "--input-nvpl"))
        {
            ctx->in_buftype = BUF_TYPE_NVPL;
        }
        else if (!strcmp(arg, "--input-nvbl"))
        {
            ctx->in_buftype = BUF_TYPE_NVBL;
        }
        else if (!strcmp(arg, "--output-raw"))
        {
            ctx->out_buftype = BUF_TYPE_RAW;
        }
        else if (!strcmp(arg, "--output-nvpl"))
        {
            ctx->out_buftype = BUF_TYPE_NVPL;
        }
        else if (!strcmp(arg, "--output-nvbl"))
        {
            ctx->out_buftype = BUF_TYPE_NVBL;
        }
        else if (!strcmp(arg, "-fm"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->flip_method = (enum v4l2_flip_method) atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->flip_method > V4L2_FLIP_METHOD_TRANS ||
                     ctx->flip_method < V4L2_FLIP_METHOD_IDENTITY),
                    "Unsupported value for flip-method: " << *argp);
        }
        else if (!strcmp(arg, "-im"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->interpolation_method =
                (enum v4l2_interpolation_method) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->interpolation_method < V4L2_INTERPOLATION_NEAREST ||
                     ctx->interpolation_method > V4L2_INTERPOLATION_BILINEAR),
                    "Unsupported value for interpolation-method: " << *argp);
        }
        else if (!strcmp(arg, "-tnr"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->tnr_algorithm = (enum v4l2_tnr_algorithm) atoi(*argp);
            CSV_PARSE_CHECK_ERROR(
                    (ctx->tnr_algorithm > V4L2_TNR_ALGO_INDOOR_HIGH_LIGHT ||
                     ctx->tnr_algorithm < V4L2_TNR_ALGO_ORIGINAL),
                    "Unsupported value for tnr algorithm: " << *argp);
        }
        else if (!strcmp(arg, "-cr"))
        {
            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->crop_rect.left = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->crop_rect.left < 0 ||
                    ctx->crop_rect.left >= (int32_t) ctx->in_width),
                    "Crop left param out of bounds");

            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->crop_rect.top = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->crop_rect.top < 0 ||
                    ctx->crop_rect.top >= (int32_t) ctx->in_height),
                    "Crop top param out of bounds");

            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->crop_rect.width = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->crop_rect.width <= 0 ||
                    ctx->crop_rect.left + ctx->crop_rect.width >
                    ctx->in_width),
                    "Crop width param out of bounds");

            argp++;
            CHECK_OPTION_VALUE(argp);
            ctx->crop_rect.height = atoi(*argp);
            CSV_PARSE_CHECK_ERROR((ctx->crop_rect.height <= 0 ||
                    ctx->crop_rect.top + ctx->crop_rect.height >
                    ctx->in_height),
                    "Crop height param out of bounds");
        }
        else
        {
            CSV_PARSE_CHECK_ERROR(ctx->out_file_path, "Unknown option " << arg);
        }
        CSV_PARSE_CHECK_ERROR(ctx->tnr_algorithm >= V4L2_TNR_ALGO_ORIGINAL &&
                ! (ctx->out_pixfmt == V4L2_PIX_FMT_YUV420M ||
                 ctx->out_pixfmt == V4L2_PIX_FMT_YUYV ||
                 ctx->out_pixfmt == V4L2_PIX_FMT_UYVY ||
                 ctx->out_pixfmt == V4L2_PIX_FMT_NV12M),
                "Unsupported format for TNR");
    }

    return 0;

error:
    print_help();
    return -1;
}
