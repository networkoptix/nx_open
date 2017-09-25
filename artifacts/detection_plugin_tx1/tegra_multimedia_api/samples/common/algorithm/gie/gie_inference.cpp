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

#include "gie_inference.h"
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <sstream>
#include <iostream>
#include <sys/stat.h>
#include <cmath>
#include <cuda_runtime_api.h>

namespace {
static const int TIMING_ITERATIONS = 1;
static const int NUM_BINDINGS = 3;
static const float THRESH = 0.8;
static const int OUTPUT_SIZE = 10;
static const int STRIDE = 4;
static const int HEL_NET_CAR = 1;
static const int INPUT_H = 540;
static const int INPUT_W = 960;
static const int CHANNEL = 3;
static const int FILTER_NUM = 6;
static const int BATCH_SIZE = 1;
static const int WORKSPACE_SIZE = 450 * 1024 * 1024;
static const int HELNET_SCALE0 = -640;
static const int HELNET_SCALE1 = -480;
static const int HELNET_SCALE2 = 640;
static const int HELNET_SCALE3 = 480;
static const int GROUP_THRESHOLD = 3;
static const float EPS = 0.2;

const char *INPUT_BLOB_NAME = "data";
const char *OUTPUT_BLOB_NAME = "coverage";
const char *OUTPUT_BBOX_NAME = "bboxes";
const char *HEL_OUTPUT_BLOB_NAME = "Layer19_cov";
const char *HEL_OUTPUT_BBOX_NAME = "Layer19_bbox";

struct float6
{
    float x;
    float y;
    float z;
    float w;
    float v;
    float u;
};

static inline float6 make_float6( float x, float y, float z, float w, float v, float u )
{
    float6 f;
    f.x = x;
    f.y = y;
    f.z = z;
    f.w = w;
    f.v = v;
    f.u = u;
    return f;
}

inline static bool rectOverlap(const float6& r1, const float6& r2)
{
    return ! ( r2.x > r1.z
        || r2.z < r1.x
        || r2.y > r1.w
        || r2.w < r1.y
        );
}

static void mergeRect( std::vector<float6>& rects, const float6& rect )
{
    const uint32_t num_rects = rects.size();

    bool intersects = false;

    for( uint32_t r=0; r < num_rects; r++ )
    {
        if( rectOverlap(rects[r], rect) )
        {
            intersects = true;

            if( rect.x < rects[r].x ) 	rects[r].x = rect.x;
            if( rect.y < rects[r].y ) 	rects[r].y = rect.y;
            if( rect.z > rects[r].z )	rects[r].z = rect.z;
            if( rect.w > rects[r].w ) 	rects[r].w = rect.w;

            break;
        }
    }

    if( !intersects )
        rects.push_back(rect);
}

} // namespace

#define CHECK(status)                                   \
{                                                       \
    if (status != 0)                                    \
    {                                                   \
        std::cout << "Cuda failure: " << status;        \
        abort();                                        \
    }                                                   \
}

// Logger for GIE info/warning/errors
class Logger : public ILogger
{
    void log(Severity severity, const char* msg) override
    {
        // suppress info-level messages
        if (severity != Severity::kINFO)
            std::cout << msg << std::endl;
    }
};


class Profiler : public IProfiler
{
    typedef std::pair<std::string, float> Record;
    std::vector<Record> mProfile;

    virtual void reportLayerTime(const char* layerName, float ms)
    {
        auto record = std::find_if(mProfile.begin(), mProfile.end(),
                        [&](const Record& r){ return r.first == layerName; });
        if (record == mProfile.end())
            mProfile.push_back(std::make_pair(layerName, ms));
        else
            record->second += ms;
    }

    void printLayerTimes()
    {
        float totalTime = 0;
        for (size_t i = 0; i < mProfile.size(); i++)
        {
            printf("%-40.40s %4.3fms\n", mProfile[i].first.c_str(),
                    mProfile[i].second / TIMING_ITERATIONS);
            totalTime += mProfile[i].second;
        }
        printf("Time over all layers: %4.3f\n", totalTime / TIMING_ITERATIONS);
    }

};

string stringtrim(string);

//This function is used to trim space
string
stringtrim(string s)
{
    int i = 0;
    while (s[i] == ' ')
    {
        i++;
    }
    s = s.substr(i);
    i = s.size()-1;
    while (s[i] == ' ')
    {
        i--;
    }

    s = s.substr(0, i + 1);
    return s;
}

unsigned int
GIE_Context::getNetWidth() const
{
    return net_width;
}

unsigned int
GIE_Context::getNetHeight() const
{
    return net_height;
}


unsigned int
GIE_Context::getFilterNum() const
{
    return filter_num;
}

void
GIE_Context::setFilterNum(const unsigned int& filter_num)
{
    this->filter_num = filter_num;
}

void*&
GIE_Context::getBuffer(const int& index)
{
    assert(index >= 0 && index < num_bindings);
    return buffers[index];
}

float*&
GIE_Context::getInputBuf()
{
    return input_buf;
}

unsigned int
GIE_Context::getNumGieInstances() const
{
    return gieinstance_num;
}

unsigned int
GIE_Context::getBatchSize() const
{
    return batch_size;
}


void
GIE_Context::setForcedFp32(const bool& forced_fp32)
{
    this->forced_fp32 = forced_fp32;
}


void
GIE_Context::setDumpResult(const bool& dump_result)
{
    this->dump_result = dump_result;
}

void
GIE_Context::setGieProfilerEnabled(const bool& enable_gie_profiler)
{
    this->enable_gie_profiler = enable_gie_profiler;
}

unsigned int
GIE_Context::getChannel() const
{
    return channel;
}


GIE_Context::GIE_Context()
{
    net_width = INPUT_W;
    net_height = INPUT_H;
    filter_num = FILTER_NUM;
    buffers = new void *[NUM_BINDINGS];
    for (int i = 0; i < NUM_BINDINGS; i++)
    {
        buffers[i] = NULL;
    }
    input_buf = NULL;
    output_cov_buf = NULL;
    output_bbox_buf = NULL;

    runtime = NULL;
    engine = NULL;
    context = NULL;
    pResultArray = new uint32_t[100*4];
    outputs = {OUTPUT_BLOB_NAME, OUTPUT_BBOX_NAME};
    thresh = THRESH;
    stride = STRIDE;

    channel = CHANNEL;
    num_bindings = NUM_BINDINGS;

    batch_size = BATCH_SIZE;

    input_blob_name = INPUT_BLOB_NAME;
    output_blob_name = OUTPUT_BLOB_NAME;
    output_bbox_name = OUTPUT_BBOX_NAME;
    gieinstance_num = 1;
    helnet_scale[0] = HELNET_SCALE0;
    helnet_scale[1] = HELNET_SCALE1;
    helnet_scale[2] = HELNET_SCALE2;
    helnet_scale[3] = HELNET_SCALE3;
    hel_net = 0;
    ped_net = 0;
    forced_fp32 = 0;
    elapsed_frame_num = 0;
    elapsed_time = 0;
    enable_gie_profiler = 1;
    dump_result = 0;
    frame_num = 0;
    result_file = "result.txt";
    pLogger = new Logger;
    pProfiler = new Profiler;
}


void
GIE_Context::allocateMemory(bool bUseCPUBuf)
{
    const ICudaEngine& cuda_engine = context->getEngine();
    // input and output buffer pointers that we pass to the engine
    // the engine requires exactly IEngine::getNbBindings() of these
    // but in this case we know that there is exactly one input and one output
    assert(cuda_engine.getNbBindings() == num_bindings);

    // In order to bind the buffers, we need to know the names of the input
    // and output tensors. note that indices are guaranteed to be less than
    // IEngine::getNbBindings()
    inputIndex = cuda_engine.getBindingIndex(input_blob_name.c_str());
    outputIndex = cuda_engine.getBindingIndex(output_blob_name.c_str());
    outputIndexBBOX = cuda_engine.getBindingIndex(output_bbox_name.c_str());
    // allocate GPU buffers
    inputDims = cuda_engine.getBindingDimensions(inputIndex);
    outputDims = cuda_engine.getBindingDimensions(outputIndex);
    outputDimsBBOX = cuda_engine.getBindingDimensions(outputIndexBBOX);

    inputSize = batch_size * inputDims.c * inputDims.h * inputDims.w *
                            sizeof(float);
    outputSize = batch_size * outputDims.c * outputDims.h *
                            outputDims.w * sizeof(float);
    outputSizeBBOX = batch_size * outputDimsBBOX.c * outputDimsBBOX.h *
                            outputDimsBBOX.w * sizeof(float);
    if (bUseCPUBuf && input_buf == NULL)
    {
        input_buf = (float *)malloc(inputSize);
        assert(input_buf != NULL);
    }

    if (output_cov_buf == NULL)
    {
        output_cov_buf = (float *)malloc(outputSize);
        assert(output_cov_buf != NULL);
    }
    if (outputIndexBBOX >= 0)
    {
        if (output_bbox_buf == NULL)
        {
            output_bbox_buf = (float *)malloc(outputSizeBBOX);
            assert(output_bbox_buf != NULL);
        }
    }
    // create GPU buffers and a stream
    if (buffers[inputIndex] == NULL)
    {
        CHECK(cudaMalloc(&buffers[inputIndex], inputSize));
    }
    if (buffers[outputIndex] == NULL)
    {
        CHECK(cudaMalloc(&buffers[outputIndex], outputSize));
    }
    if (outputIndexBBOX >= 0)
    {
        if (buffers[outputIndexBBOX] == NULL)
        {
            CHECK(cudaMalloc(&buffers[outputIndexBBOX], outputSizeBBOX));
        }
    }
    if (dump_result)
    {
        fstream.open(result_file.c_str(), ios::out);
    }
}

void
GIE_Context::releaseMemory(bool bUseCPUBuf)
{
    for (int i = 0; i < NUM_BINDINGS; i++)
    {
        if (buffers[i] != NULL)
        {
            CHECK(cudaFree(buffers[i]));
            buffers[i] = NULL;
        }
    }
    if (bUseCPUBuf && input_buf != NULL)
    {
        free(input_buf);
        input_buf = NULL;
    }
    if (output_cov_buf != NULL)
    {
        free(output_cov_buf);
        output_cov_buf = NULL;
    }
    if (output_bbox_buf != NULL)
    {
        free(output_bbox_buf);
        output_bbox_buf = NULL;
    }

    if (pResultArray != NULL)
    {
        delete []pResultArray;
        pResultArray = NULL;
    }

    if (dump_result)
    {
        fstream.close();
    }
}

GIE_Context::~GIE_Context()
{

    delete pLogger;
    delete pProfiler;
    delete []buffers;
}

void
GIE_Context::caffeToGIEModel(const string& deployfile, const string& modelfile)
{
    // create API root class - must span the lifetime of the engine usage
    IBuilder *builder = createInferBuilder(*pLogger);
    INetworkDefinition *network = builder->createNetwork();

    // parse the caffe model to populate the network, then set the outputs
    ICaffeParser *parser = createCaffeParser();

    bool hasFp16 = builder->platformHasFastFp16();

    // if user specify
    if (forced_fp32 == false)
    {
        if (hasFp16)
        {
            printf("forced_fp32 has been set to 0(using fp16)\n");
        }
        else
        {
            printf("platform don't have fp16, force to 1(using fp32)\n");
        }
    }
    else if(forced_fp32 == true)
    {
        printf("forced_fp32 has been set to 1(using fp32)\n");
        hasFp16 = 0;
    }

    // create a 16-bit model if it's natively supported
    DataType modelDataType = hasFp16 ? DataType::kHALF : DataType::kFLOAT;
    const IBlobNameToTensor *blobNameToTensor =
        parser->parse(deployfile.c_str(),               // caffe deploy file
                                 modelfile.c_str(),     // caffe model file
                                 *network,              // network definition that parser populate
                                 modelDataType);

    assert(blobNameToTensor != nullptr);
    // the caffe file has no notion of outputs
    // so we need to manually say which tensors the engine should generate
    for (auto& s : outputs)
    {
        network->markOutput(*blobNameToTensor->find(s.c_str()));
    }

    // Build the engine
    builder->setMaxBatchSize(batch_size);
    builder->setMaxWorkspaceSize(WORKSPACE_SIZE);

    // Eliminate the side-effect from the delay of GPU frequency boost
    builder->setMinFindIterations(3);
    builder->setAverageFindIterations(2);

    // set up the network for paired-fp16 format, only on DriveCX
    if (hasFp16)
    {
        builder->setHalf2Mode(true);
    }

    ICudaEngine *engine = builder->buildCudaEngine(*network);
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

void
GIE_Context::buildGieContext(const string& deployfile,
        const string& modelfile, bool bUseCPUBuf)
{
    if (!parseNet(deployfile))
    {
        cout<<"parse net failed, exit!"<<endl;
        exit(0);
    }
    ifstream gieModelFile("gieModel.cache");
    if (gieModelFile.good())
    {
        cout<<"Using cached GIE model"<<endl;
        gieModelStream << gieModelFile.rdbuf();
    }
    else
    {
        caffeToGIEModel(deployfile, modelfile);
        cout<<"Create GIE model cache"<<endl;
        ofstream gieModelFile("gieModel.cache");
        gieModelFile << gieModelStream.rdbuf();
        gieModelFile.close();
    }
    gieModelStream.seekg(0, gieModelStream.beg);
    runtime = createInferRuntime(*pLogger);
    engine = runtime->deserializeCudaEngine(gieModelStream);
    context = engine->createExecutionContext();
    allocateMemory(bUseCPUBuf);
}

void
GIE_Context::destroyGieContext(bool bUseCPUBuf)
{
    releaseMemory(bUseCPUBuf);
    context->destroy();
    engine->destroy();
    runtime->destroy();
}

void
GIE_Context::doInference(
    queue< vector<cv::Rect> >& rectList_queue,
    float *input)
{
    struct timeval input_time;
    struct timeval output_time;

    if (!enable_gie_profiler)
    {
        cudaStream_t stream;
        CHECK(cudaStreamCreate(&stream));

        // DMA the input to the GPU,  execute the batch asynchronously
        // and DMA it back
        if (input != NULL)   //NULL means we have use GPU to map memory
        {
            CHECK(cudaMemcpyAsync(buffers[inputIndex], input, inputSize,
                                cudaMemcpyHostToDevice, stream));
        }

        context->enqueue(batch_size, buffers, stream, nullptr);
        CHECK(cudaMemcpyAsync(output_cov_buf, buffers[outputIndex], outputSize,
                                cudaMemcpyDeviceToHost, stream));
        if (outputIndexBBOX >= 0)
        {
            CHECK(cudaMemcpyAsync(output_bbox_buf, buffers[outputIndexBBOX],
                            outputSizeBBOX, cudaMemcpyDeviceToHost, stream));
        }

        cudaStreamSynchronize(stream);

        // release the stream and the buffers
        cudaStreamDestroy(stream);
    }
    else
    {
        // DMA the input to the GPU,  execute the batch synchronously
        // and DMA it back
        if (input != NULL)   //NULL means we have use GPU to map memory
        {
            CHECK(cudaMemcpy(buffers[inputIndex], input, inputSize,
                                cudaMemcpyHostToDevice));
        }

        gettimeofday(&input_time, NULL);
        context->execute(batch_size, buffers);
        gettimeofday(&output_time, NULL);
        CHECK(cudaMemcpy(output_cov_buf, buffers[outputIndex], outputSize,
                                cudaMemcpyDeviceToHost));
        if (outputIndexBBOX >= 0)
        {
            CHECK(cudaMemcpy(output_bbox_buf, buffers[outputIndexBBOX],
                            outputSizeBBOX, cudaMemcpyDeviceToHost));
        }
        elapsed_frame_num += batch_size;
        elapsed_time += (output_time.tv_sec - input_time.tv_sec) * 1000 +
                        (output_time.tv_usec - input_time.tv_usec) / 1000;
        if (elapsed_frame_num >= 100)
        {
            printf("Time elapsed:%ld ms per frame in past %ld frames\n",
                elapsed_time / elapsed_frame_num, elapsed_frame_num);
            elapsed_frame_num = 0;
            elapsed_time = 0;
        }
    }

    vector<cv::Rect> rectList;
    for (unsigned int i = 0; i < batch_size; i++)
    {
        if (ped_net)
        {
            printf("The net is ped_net\n");
            parsePed100Bbox(rectList, i);
        }
        else if (hel_net)
        {
            printf("The net is hel_net\n");
            //hel net is multiclass,  here we only fetch car
            parseHelBbox(rectList, HEL_NET_CAR, i);
        }
        else
        {
            printf("The net is for cars\n");
            parseBbox(rectList, i);
        }


        if (dump_result)
        {
            fstream << "frame:" << frame_num++ << " has rect:"
                    << rectList.size() << endl;
            for (unsigned int i = 0; i < rectList.size(); i++)
            {
                fstream << "\tx,y,w,h:"
                    << (float) rectList[i].x / net_width << " "
                    << (float) rectList[i].y / net_height << " "
                    << (float) rectList[i].width / net_width << " "
                    << (float) rectList[i].height / net_height << endl;
            }
            fstream << endl;
        }
        rectList_queue.push(rectList);
    }
}

void
GIE_Context::parseBbox(vector<cv::Rect>& rectList, int batch_th)
{
    int gridsize = outputDims.h * outputDims.w;
    float *output_x1 = output_bbox_buf +
            outputDimsBBOX.c * outputDimsBBOX.h * outputDimsBBOX.w * batch_th;
    float *output_y1 = output_x1 + outputDimsBBOX.h * outputDimsBBOX.w;
    float *output_x2 = output_y1 + outputDimsBBOX.h * outputDimsBBOX.w;
    float *output_y2 = output_x2 + outputDimsBBOX.h * outputDimsBBOX.w;
    int gridoffset = outputDims.c * outputDims.h * outputDims.w * batch_th;

    for (int i = 0; i < gridsize; ++i)
    {
        if (output_cov_buf[gridoffset + i] >= thresh)
        {
            int g_x = i % outputDims.w;
            int g_y = i / outputDims.w;
            int i_x = g_x * stride ;
            int i_y = g_y * stride ;
            int rectx1 = output_x1[i]+i_x;
            int recty1 = output_y1[i]+i_y;
            int rectx2 = output_x2[i]+i_x;
            int recty2 = output_y2[i]+i_y;
            if (rectx1 < 0)
            {
                rectx1 = 0;
            }
            if (rectx2 < 0)
            {
                rectx2 = 0;
            }
            if (recty1 < 0)
            {
                recty1 = 0;
            }
            if (recty2 < 0)
            {
                recty2 = 0;
            }

            if (rectx1 >= (int)net_width)
            {
                rectx1 = net_width - 1;
            }
            if (rectx2 >= (int)net_width)
            {
                rectx2 = net_width - 1;
            }
            if (recty1 >= (int)net_height)
            {
                recty1 = net_height - 1;
            }
            if (recty2 >= (int)net_height)
            {
                recty2 = net_height - 1;
            }

            rectList.push_back(cv::Rect(rectx1, recty1,
                        rectx2-rectx1, recty2-recty1));
        }
    }

    cv::groupRectangles(rectList, GROUP_THRESHOLD, EPS);
}

void
GIE_Context::parsePed100Bbox(vector<cv::Rect>& rectList, int batch_th)
{
    auto ow = outputDimsBBOX.w;
    auto oh = outputDimsBBOX.h;
    auto owh = ow * oh;

    auto cellWidth = 1024 / ow;
    auto cellHeight = 512 / oh;

    for (auto y = 0; y < oh; ++y)
    {
        for (auto x = 0; x < ow; ++x)
        {
            const float coverage = output_cov_buf[ow * y + x];

#if 0
            if (y * ow + x < 100) 
                std::cout << "coverage: " << coverage << std::endl;
#endif

            if (coverage > thresh)
            {
                const float mx = x * cellWidth;
                const float my = y * cellHeight;

                auto x1 = output_bbox_buf[0 * owh + y * ow + x] + mx;
                auto y1 = output_bbox_buf[1 * owh + y * ow + x] + my;
                auto x2 = output_bbox_buf[2 * owh + y * ow + x] + mx;
                auto y2 = output_bbox_buf[3 * owh + y * ow + x] + my;

                rectList.push_back(cv::Rect((int)x1, (int)y1, (int)(x2 -x1), (int)(y2 - y1)));
            }
        }
    }

    cv::groupRectangles(rectList, GROUP_THRESHOLD, EPS);
}

void
GIE_Context::parseHelBbox(
                    vector<cv::Rect>& rectList, int class_num, int batch_th)
{
    int grid_x_ = outputDims.w;
    int grid_y_ = outputDims.h;
    int gridsize_ = grid_x_ * grid_y_;

    for (int c = 0; c < class_num; c++)
    {
        float *output_x1 = output_bbox_buf +
                c * 4 * outputDimsBBOX.h * outputDimsBBOX.w +
                outputDimsBBOX.c * outputDimsBBOX.h *
                outputDimsBBOX.w * batch_th;
        float *output_y1 = output_x1 + outputDims.h * outputDims.w;
        float *output_x2 = output_y1 + outputDims.h * outputDims.w;
        float *output_y2 = output_x2 + outputDims.h * outputDims.w;
        int gridoffset = outputDims.c * outputDims.h * outputDims.w * batch_th;

        for (int i = 0; i < gridsize_; i++)
        {
            if (output_cov_buf[gridoffset + c * gridsize_ + i] >= thresh)
            {
                int g_x = i % grid_x_;
                int g_y = i / grid_x_;
                int i_x = g_x * stride;
                int i_y = g_y * stride;
                int rectx1 = helnet_scale[0] * output_x1[i] + i_x;
                int recty1 = helnet_scale[1] * output_y1[i] + i_y;
                int rectx2 = helnet_scale[2] * output_x2[i] + i_x;
                int recty2 = helnet_scale[3] * output_y2[i] + i_y;

                if (rectx1 < 0)
                {
                    rectx1 = 0;
                }
                if (rectx2 < 0)
                {
                    rectx2 = 0;
                }
                if (recty1 < 0)
                {
                    recty1 = 0;
                }
                if (recty2 < 0)
                {
                    recty2 = 0;
                }

                if (rectx1 >= (int)net_width)
                {
                    rectx1 = net_width - 1;
                }
                if (rectx2 >= (int)net_width)
                {
                    rectx2 = net_width - 1;
                }
                if (recty1 >= (int)net_height)
                {
                    recty1 = net_height - 1;
                }
                if (recty2 >= (int)net_height)
                {
                    recty2 = net_height - 1;
                }

                rectList.push_back(cv::Rect(rectx1, recty1,
                    rectx2 - rectx1, recty2 - recty1));
            }
        }

        cv::groupRectangles(rectList, GROUP_THRESHOLD, EPS);
    }
}

int
GIE_Context::parseNet(const string& deployfile)
{
    ifstream readfile;
    string line;
    readfile.open(deployfile, ios::in);
    if (!readfile)
    {
        return 0;
    }
    int iterator = 0;

    bool input_shape_found = false;
    while (1)
    {
        if (!getline(readfile, line))
            return 0;

        string::size_type index;        
        
        index = line.find("input_dim");
        if (!input_shape_found && index ==std::string::npos)
        {
            index = line.find("input_shape");
            if (index != std::string::npos)
            {
                input_shape_found = true;
            }
            continue;
        }

        index = line.find_first_of(":", 0);
        string first = line.substr(0, index);
        string second = line.substr(index + 1);
        switch(iterator)
        {
        case 0:  //for batch size
            batch_size = atoi(stringtrim(second).c_str());
            break;

        case 1:  // for channel num in net
            channel = atoi(stringtrim(second).c_str());
            break;

        case 2:  // for net's height
            net_height = atoi(stringtrim(second).c_str());
            break;
        case 3:  // for net's width
            net_width = atoi(stringtrim(second).c_str());
            break;

        default:
            break;
        }

        if (iterator == 3)
        {
            break;
        }

        iterator++;
    }

    cout<<"Net has batch_size, channel, net_height, net_width:" <<
        batch_size << " " << channel << " " << net_height << " " <<
        net_width << endl;

    if (net_height == 480 && net_width == 640)
    {
        hel_net = 1;
    }
    else if (net_height == 512 && net_width == 1024)
    {
        ped_net = 1;
    }

    if (hel_net)
    {
        output_blob_name = HEL_OUTPUT_BLOB_NAME;
        output_bbox_name = HEL_OUTPUT_BBOX_NAME;
        outputs = {HEL_OUTPUT_BLOB_NAME, HEL_OUTPUT_BBOX_NAME};
        stride = 16;
    }
    readfile.close();

    return 1;
}
