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
#ifndef GIE_INFERENCE_H_
#define GIE_INFERENCE_H_

#include <fstream>
#include <queue>
#include "NvInfer.h"
#include "NvCaffeParser.h"
#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <opencv2/objdetect/objdetect.hpp>
using namespace nvinfer1;
using namespace nvcaffeparser1;
using namespace std;

class Logger;

class Profiler;

class GIE_Context
{
public:
    //net related parameter
    unsigned int getNetWidth() const;

    unsigned int getNetHeight() const;

    unsigned int getBatchSize() const;

    unsigned int getChannel() const;

    // Buffer is allocated in GIE_Conxtex,
    // Expose this interface for inputing data
    void*& getBuffer(const int& index);

    float*& getInputBuf();

    unsigned int getNumGieInstances() const;

    void setForcedFp32(const bool& forced_fp32);

    void setDumpResult(const bool& dump_result);

    void setGieProfilerEnabled(const bool& enable_gie_profiler);

    unsigned int getFilterNum() const;
    void setFilterNum(const unsigned int& filter_num);

    GIE_Context();

    void buildGieContext(
        const string& deployfile,
        const string& modelfile,
        const string& cachefile,
        bool bUseCPUBuf = false);

    void doInference(
        queue< vector<cv::Rect> >& rectList_queue,
        float *input = NULL);

    void destroyGieContext(bool bUseCPUBuf = false);

    ~GIE_Context();

private:
    unsigned int net_width;
    unsigned int net_height;
    unsigned int filter_num;
    void  **buffers;
    float *input_buf;
    float *output_cov_buf;
    float *output_bbox_buf;
    float helnet_scale[4];
    int hel_net;
    int ped_net;
    IRuntime *runtime;
    ICudaEngine *engine;
    IExecutionContext *context;
    uint32_t *pResultArray;
    unsigned int channel;              //input file's channel
    int num_bindings;
    unsigned int gieinstance_num;        //inference channel num
    unsigned int batch_size;
    bool forced_fp32;
    bool dump_result;
    ofstream fstream;
    bool enable_gie_profiler;
    stringstream gieModelStream;
    vector<string> outputs;
    string result_file;
    Logger *pLogger;
    Profiler *pProfiler;
    unsigned int frame_num;
    float  thresh;
    unsigned int stride;
    string input_blob_name;
    string output_blob_name;
    string output_bbox_name;
    uint64_t elapsed_frame_num;
    uint64_t elapsed_time;
    int inputIndex;
    int outputIndex;
    int outputIndexBBOX;
    Dims3 inputDims;
    Dims3 outputDims;
    Dims3 outputDimsBBOX;
    size_t inputSize;
    size_t outputSize;
    size_t outputSizeBBOX;

    int parseNet(const string& deployfile);

    void parseBbox(vector<cv::Rect>& rectList, int batch_th);
    void parsePed100Bbox(vector<cv::Rect>& rectList, int batch_th);

    void parseHelBbox(vector<cv::Rect>& rectList, int class_num, int batch_th);
    void allocateMemory(bool bUseCPUBuf);
    void releaseMemory(bool bUseCPUBuf);
    void caffeToGIEModel(const string& deployfile, const string& modelfile);
};

#endif
