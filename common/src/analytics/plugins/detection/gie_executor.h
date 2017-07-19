#pragma once
#if defined(ENABLE_NVIDIA_ANALYTICS)

#include <QtCore/QString>

#include <vector>

#include <NvInfer.h>
#include <NvCaffeParser.h>
#include <cuda_runtime_api.h>

namespace nx {
namespace analytics {

const int kCarNetWidth = 960;
const int kCarNetHeight = 540;

const int kPedNetWidth = 1024;
const int kPedNetHeight = 512;

struct BufferInfo
{
    BufferInfo() = default;
    BufferInfo(const QString& bufferName): name(bufferName) {}

    QString name;
    int index = -1;
    std::size_t size = 0;
    nvinfer1::Dims3 dimensions;
};

class GieLogger: public nvinfer1::ILogger
{
    void log(nvinfer1::ILogger::Severity severity, const char* message)
    {
        //qDebug() << "====> GIE message:" << message;
    }
};

class GieExecutor
{
public:
    GieExecutor(const QString& inputBlobName, std::vector<QString> outputBlobNames);

    ~GieExecutor();

    bool buildEngine();
    void doInference(bool useCudaInputBuffer = false);
    void* getBuffer(int index) const;
    float* getInputBuffer() const;

    nvinfer1::Dims3 getOutputDimensions(int bufferIndex) const;

    int getNetWidth() const;
    int getNetHeight() const;
    int getChannelNumber() const;

    void* getCudaInputBuffer() { return m_deviceBuffers[m_input.index]; }

private:
    void allocateMemory(bool useCpuBuffers);
    void releaseMemory();

private:
    BufferInfo m_input;
    std::vector<BufferInfo> m_outputs;

    GieLogger m_logger;
    nvinfer1::ICudaEngine* m_cudaEngine;
    nvinfer1::IExecutionContext* m_executionContext;

    std::vector<void*> m_deviceBuffers;
    std::vector<float*> m_hostBuffers;
};

} // namespace analytics
} // namespace nx

#endif // defined(ENABLE_NVIDIA_ANALYTICS)
