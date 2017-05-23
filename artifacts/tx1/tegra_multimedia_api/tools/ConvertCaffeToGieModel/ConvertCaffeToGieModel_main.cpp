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
#include <sstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef ENABLE_GIE
#include "NvInfer.h"
#include "NvCaffeParser.h"
#include "gieModel.h"

using namespace nvinfer1;
using namespace nvcaffeparser1;

class Logger : public ILogger {
    void log(Severity severity, const char *msg) override
    {
        if (severity != Severity::kINFO)
            std::cout << msg << std::endl;
    }
};

class Logger gLogger;

enum net_mode { Classification, Detection } Mode;

void caffeToGIEModel (const std::string& deployFile,
                     const std::string& modelFile,
                     // network outputs
                     const std::vector<std::string>& outputs,
                     unsigned int batchSize,
                     unsigned int workSpaceSize,
                     std::ostream& gieModelStream,
                     int force_use_fp16)
{
    // create API root class - must span the lifetime of the engine usage
    IBuilder* builder = createInferBuilder(gLogger);
    INetworkDefinition* network = builder->createNetwork();

    // parse the caffe model to populate the network, then set the outputs
    ICaffeParser* parser = createCaffeParser();

    bool useFp16 = builder->platformHasFastFp16();

    if (! force_use_fp16)
        useFp16 = 0;

    std::cout << "useFp16: " << useFp16 << std::endl;
    // create a 16-bit model if it's natively supported
    DataType modelDataType = useFp16 ? DataType::kHALF : DataType::kFLOAT;
    const IBlobNameToTensor *blobNameToTensor =
              parser->parse(deployFile.c_str(),
                                       modelFile.c_str(),
                                       *network,
                                       modelDataType);

    assert(blobNameToTensor != nullptr);
    // the caffe file has no notion of outputs, so we need to manually say
    // which tensors the engine should generate
    for (auto& s : outputs)
        network->markOutput(*blobNameToTensor->find(s.c_str()));

    // Build the engine
    builder->setMaxBatchSize(batchSize);
    builder->setMaxWorkspaceSize(workSpaceSize);

    // Eliminate the side-effect from the delay of GPU frequency boost
    builder->setMinFindIterations(3);
    builder->setAverageFindIterations(2);

    // set up the network for paired-fp16 format, only on DriveCX
    if(useFp16)
        builder->setHalf2Mode(true);

    ICudaEngine* engine = builder->buildCudaEngine(*network);
    assert(engine);

    // we don't need the network any more, and we can destroy the parser
    network->destroy();
    parser->destroy();

    // serialize the engine, then close everything down
    engine->serialize(gieModelStream);
    engine->destroy();
    builder->destroy();
    shutdownProtobufLibrary();
}

static void usages(char *cmd)
{
    printf("%s\n"
           "    -n <network_name>\n"
           "    -l <model_name>\n"
           "    -m <mode_name>\n"
           "    -o <outBlob_name>[,<outBlob_name>]\n"
           "    -f <fp16 or fp32>\n"
           "    -b <batch_size>\n"
           "    -w <workspace_size>\n"
           "    -s <store_cache_name>\n", cmd);

    printf("For Example: \n");

    printf("    %s -h\n", cmd);

    printf("    %s -n ../../data/model/GoogleNet-modified.prototxt"
                 " -l ../../data/model/GoogleNet-modified-online_iter_30000.caffemodel"
                 " -m detection -o coverage,bboxes -f fp16 -b 2"
                 " -w 115343360"
                 " -s gieModel.cache\n", cmd);
}

static void parse_command(int argc, char** argv, GieModel *gieModel)
{
    int cc = 0;
    if (argc == 1)
    {
        printf("Please use: %s -h\n", argv[0]);
        exit(0);
    }
    while ((cc = getopt (argc, argv, "n:l:m:o:f:b:w:s:h")) != -1)
    {
        switch (cc)
        {
            case 'n':
                gieModel->input_file_name = optarg;
                if (gieModel->input_file_name == NULL)
                {
                    usages(argv[0]);
                    exit(0);
                }
                break;
            case 'l':
                gieModel->input_model_name = optarg;
                if (gieModel->input_model_name == NULL)
                {
                    usages(argv[0]);
                    exit(0);
                }
                break;
            case 'm':
                gieModel->mode_name = optarg;
                if (gieModel->mode_name == NULL)
                {
                    usages(argv[0]);
                    exit(0);
                }
                if (0 == strcmp(optarg, "classification"))
                {
                    Mode = Classification;
                }
                else if (0 == strcmp(optarg, "detection"))
                {
                    Mode = Detection;
                }
                else
                {
                    usages(argv[0]);
                    exit(0);
                }
                break;
            case 'o':
                gieModel->outBlob_name = optarg;
                if (gieModel->outBlob_name== NULL)
                {
                    usages(argv[0]);
                    exit(0);
                }
                break;
            case 'f':
                gieModel->fp_mode_p = optarg;
                if (gieModel->fp_mode_p == NULL)
                {
                    usages(argv[0]);
                    exit(0);
                }
                if (0 == strcmp(optarg, "fp16"))
                {
                    gieModel->is_fp16 = 1;
                }
                else if (0 == strcmp(optarg, "fp32"))
                {
                    gieModel->is_fp16 = 0;
                }
                else
                {
                    usages(argv[0]);
                    exit(0);
                }
                break;
            case 'b':
                gieModel->batch_size_p = optarg;
                if (gieModel->batch_size_p == NULL)
                {
                    usages(argv[0]);
                    exit(0);
                }
                gieModel->batch_size = atoi(optarg);
                if (gieModel->batch_size == 0)
                {
                    printf ("batch size cannot be 0\n");
                    exit(0);
                }
                break;
            case 'w':
                gieModel->workspace_size_p = optarg;
                if (gieModel->workspace_size_p == NULL)
                {
                    usages(argv[0]);
                    exit(0);
                }
                gieModel->workspace_size = atoi(optarg);
                if (gieModel->workspace_size == 0)
                {
                    printf ("workspace size cannot be 0\n");
                    exit(0);
                }
                break;
            case 's':
                gieModel->store_cached_file_name = optarg;
                if (gieModel->store_cached_file_name == NULL)
                {
                    usages(argv[0]);
                    exit(0);
                }
                break;
            case 'h':
                usages(argv[0]);
                exit(0);
                break;
            case '?':
                if (optopt == 'n' || optopt == 'f' || optopt == 'b')
                    fprintf (stderr, "Option -%c requires an argument.\n",
                               optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr, "Unknown option character `\\x%x'.\n",
                               optopt);
                usages(argv[0]);
                exit(0);
            default:
                usages(argv[0]);
                exit(0);
        }
    }
    std::cout << "input_file_name: " << gieModel->input_file_name << std::endl;
    std::cout << "input_model_name: " << gieModel->input_model_name << std::endl;
    std::cout << "mode_name: " << gieModel->mode_name << std::endl;
    std::cout << "outBlob_name: " << gieModel->outBlob_name << std::endl;
    std::cout << "is_fp16: " << gieModel->is_fp16 << std::endl;
    std::cout << "batch_size: " << gieModel->batch_size << std::endl;
    std::cout << "workspace_size: " << gieModel->workspace_size << std::endl;
    std::cout << "store_cached_file_name: " << gieModel->store_cached_file_name << std::endl;
    return;
}
#endif
int main(int argc, char** argv)
{
#ifdef ENABLE_GIE
    struct timeval tv;
    struct timezone tz;
    struct timeval tv1;
    struct timezone tz1;
    struct GieModel gieModel;

    parse_command(argc, argv, &gieModel);

    std::cout << "Building a GPU inference engine with batch size = "
              << gieModel.batch_size<< std::endl;

    std::stringstream gieModelStream;
    gieModelStream.seekg(0, gieModelStream.beg);

    // collect output blobs information
    std::vector < std::string > outBlobVector;
    if (Mode == Detection)
    {
        gieModel.ptr_p = strchr(gieModel.outBlob_name,',');
        if (gieModel.ptr_p == NULL)
        {
            usages(argv[0]);
            exit(0);
        }

        memset(gieModel.ptr,0,sizeof(gieModel.ptr));
        memcpy(gieModel.ptr, gieModel.outBlob_name,
                (gieModel.ptr_p-gieModel.outBlob_name));
        std::cout << "outBlob_name: "<<gieModel.ptr<< std::endl;
        outBlobVector.push_back(gieModel.ptr);

        memset(gieModel.ptr,0,sizeof(gieModel.ptr));
        memcpy(gieModel.ptr, gieModel.ptr_p+1, strlen(gieModel.ptr_p+1));
        std::cout << "outBlob_name: "<<gieModel.ptr<< std::endl;
        outBlobVector.push_back(gieModel.ptr);
    }
    else
    {
        std::cout << "outBlob_name: "<< gieModel.outBlob_name << std::endl;
        outBlobVector.push_back(gieModel.outBlob_name);
    }

    gettimeofday(&tv, &tz);
    // convert to gie model
    caffeToGIEModel(
        gieModel.input_file_name,
        gieModel.input_model_name,
        outBlobVector,
        gieModel.batch_size,
        gieModel.workspace_size,
        gieModelStream,
        gieModel.is_fp16);
    gettimeofday(&tv1, &tz1);
    std::cout << "model conversion time: "
              << ((tv1.tv_sec-tv.tv_sec)*1000+(tv1.tv_usec-tv.tv_usec)/1000)
              << " ms" << std::endl;

    // cache the gie model
    std::ofstream gieModelFile(gieModel.store_cached_file_name);
    gieModelFile << gieModelStream.rdbuf();
    gieModelFile.close();
    std::cout <<"Cache GIE model to gieModel.cache"<< std::endl;
#endif
    std::cout << "Done." << std::endl;

    return 0;
}
