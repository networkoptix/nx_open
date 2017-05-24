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

#include "camera_caffe_main.h"

#define CHECK_OPTION_VALUE(argp) \
    if(!*argp || (*argp)[0] == '-') \
    { \
        cerr << "Error: value not specified for option " << arg << endl; \
        goto error; \
    }

#define CSV_PARSE_CHECK_ERROR(condition, str) \
    if (condition) \
    { \
    cerr << "Error: " << str << endl; \
    goto error; \
    }



using namespace std;

static void print_help(const char *app_name)
{
    cerr << "Usage:" << app_name << " [options]\n\n";
    cerr << "OPTIONS:\n";
    cerr << "\t-h,--help\n\n";

    cerr << "\t-width width                camera width, <1 -- 4096>\n";
    cerr << "\t-height height              camera height, <1 -- 4096>\n\n";

    cerr << "\t-lib library                image processing library\n\n";

    cerr << "\t-model model_file           CAFFE model file\n";
    cerr << "\t-trained trained_file       CAFFE trained file\n";
    cerr << "\t-mean mean_file             CAFFE mean file\n";
    cerr << "\t-label label_file           CAFFE label file\n";

    return;
}

int parse_csv_args(camera_caffe_context *ctx, int argc, const char *argv[])
{
    char **argp = (char **) argv;
    char *arg = (char *) argv[0];

    if (argc == 1 || (arg && (!strcmp(arg, "-h") || !strcmp(arg, "--help"))))
    {
        print_help(argv[0]);
        exit(EXIT_SUCCESS);
    }

    while ((arg = *(++argp)) != NULL)
    {
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            print_help(argv[0]);
            exit(EXIT_SUCCESS);
        }
        else if (!strcmp(arg, "-width"))
        {
            arg = *(++argp);
            ctx->width = atoi(arg);
        }
        else if (!strcmp(arg, "-height"))
        {
            arg = *(++argp);
            ctx->height = atoi(arg);
        }
        else if (!strcmp(arg, "-lib"))
        {
            arg = *(++argp);
            ctx->lib_file = strdup(arg);
        }
        else if (!strcmp(arg, "-model"))
        {
            arg = *(++argp);
            ctx->model_file = strdup(arg);
        }
        else if (!strcmp(arg, "-trained"))
        {
            arg = *(++argp);
            ctx->trained_file = strdup(arg);
        }
        else if (!strcmp(arg, "-mean"))
        {
            arg = *(++argp);
            ctx->mean_file = strdup(arg);
        }
        else if (!strcmp(arg, "-label"))
        {
            arg = *(++argp);
            ctx->label_file = strdup(arg);
        }
        else
        {
            CSV_PARSE_CHECK_ERROR(1, "Unknown option " << arg);
        }
    }

    /* check paramters */
    CSV_PARSE_CHECK_ERROR(((ctx->width <= 0) || (ctx->width >= 4096))
        , "Input width should be among 0-4096");
    CSV_PARSE_CHECK_ERROR(((ctx->height <= 0) || (ctx->height > 4096))
        , "Input height should be among 0-4096");
    CSV_PARSE_CHECK_ERROR(!ctx->lib_file, "lib file missing!\n");
    CSV_PARSE_CHECK_ERROR(!ctx->model_file, "CAFFE modle file missing!\n");
    CSV_PARSE_CHECK_ERROR(!ctx->trained_file, "CAFFE trained file missing!\n");
    CSV_PARSE_CHECK_ERROR(!ctx->mean_file, "CAFFE mean file missing!\n");
    CSV_PARSE_CHECK_ERROR(!ctx->label_file, "CAFFE label file missing!\n");
    return 0;

error:
    print_help(argv[0]);
    return -1;

}

