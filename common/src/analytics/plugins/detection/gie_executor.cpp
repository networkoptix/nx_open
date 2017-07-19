#include "gie_executor.h"
#if defined(ENABLE_NVIDIA_ANALYTICS)

#include <analytics/common/config.h>

#include <QFileInfo>
#include <QtCore/QCoreApplication>

#include <fstream>
#include <iostream>
#include <ios>

#include <QElapsedTimer>

#include <nx/utils/log/assert.h>
#include <utils/common/app_info.h>

namespace {

const int kBatchSize  = 1;
const int kWorkspaceSize = 10 * 1024 * 1024; //< Where does this number come from?

const int kMinFindIterations = 3;
const int kAvgFindIterations = 2;

const int kNumBindings = 3;

const int kChannelNumber = 3;

} // namespace

using namespace nvinfer1;
using namespace nvcaffeparser1;

namespace nx {
namespace analytics {

GieExecutor::GieExecutor(const QString& inputBlobName, std::vector<QString> outputBlobNames):
    m_input(inputBlobName)
{
    for (const auto& outputName: outputBlobNames)
        m_outputs.emplace_back(outputName);
}

GieExecutor::~GieExecutor()
{
    m_executionContext->destroy();
    m_cudaEngine->destroy();
    releaseMemory();
}

bool GieExecutor::buildEngine()
{
    qDebug() << "=========================>";
    qDebug() << "=========================> Trying to build GIE engine (C)"
        << conf.cacheFile << "(D)" << conf.deployFile << "(M)" << conf.modelFile;
    qDebug() << "=========================>";

    QFileInfo info(QLatin1String(conf.cacheFile));
    if (!info.exists())
    {
        qDebug() << "=============> No cache file in" << conf.cacheFile;
        IBuilder* builder = createInferBuilder(m_logger);
        INetworkDefinition* network = builder->createNetwork();
        ICaffeParser* parser = createCaffeParser();

        bool hasFp16 = builder->platformHasFastFp16();
        qDebug() << "=============> HAS FP 16" << hasFp16;
        //bool hasFp16 = false; //< Requires blob size of 2. Isn't supported at this moment.

        DataType modelDataType = hasFp16 ? DataType::kHALF : DataType::kFLOAT;

        qDebug() << "===================> Loading model files from"
            << conf.deployFile << conf.modelFile;
        const IBlobNameToTensor* blobNameToTensor = parser->parse(
            conf.deployFile, conf.modelFile, *network, modelDataType);

        NX_ASSERT(blobNameToTensor, lit("Can't parse Caffe model."));

        for (auto& output: m_outputs)
            network->markOutput(*blobNameToTensor->find(output.name.toStdString().c_str()));

        builder->setMaxBatchSize(kBatchSize);
        builder->setMaxWorkspaceSize(kWorkspaceSize);

        // Number of minimization iterations (don't know what it is).
        builder->setMinFindIterations(kMinFindIterations);

        // Number of averaging iterations (don't know what it is).
        builder->setAverageFindIterations(kAvgFindIterations);

        builder->setHalf2Mode(hasFp16);

        m_cudaEngine = builder->buildCudaEngine(*network);

        NX_ASSERT(m_cudaEngine, lit("Can't build CUDA engine."));
        if (!m_cudaEngine)
            return false;

        std::ofstream cudaEngineStream;
        cudaEngineStream.open(conf.cacheFile, std::ios::out|std::ios::trunc);

        NX_ASSERT(
            cudaEngineStream.is_open(),
            lit("Couldn't open CUDA engine cache file. Unable to serialize engine."));
        if (!cudaEngineStream.is_open())
            return false;

        m_cudaEngine->serialize(cudaEngineStream);

        // Cleanup. We don't need the network, the parser, and the builder anymore
        network->destroy();
        parser->destroy();
        builder->destroy();
    }
    else
    {
        qDebug() << "============> FILE INFO EXISTS" << conf.cacheFile;
        std::ifstream cudaEngineStream;
        cudaEngineStream.open(conf.cacheFile, std::ios::in);

        NX_ASSERT(
            cudaEngineStream.is_open(),
            lit("Couldn't open CUDA engine cache file. Unable to deserialize engine."));

        if (!cudaEngineStream.is_open())
            return false;

        auto runtime = createInferRuntime(m_logger);
        m_cudaEngine = runtime->deserializeCudaEngine(cudaEngineStream);
        runtime->destroy();
    }

    m_executionContext = m_cudaEngine->createExecutionContext();

    NX_ASSERT(m_executionContext, lit("Can't create execution context"));
    if (!m_executionContext)
        return false;

    allocateMemory(true);

    qDebug() << "==================>";
    qDebug() << "==================> GIE ENGINE HAS BEEN BUILT!";
    qDebug() << "==================>";

    return true;
}

void GieExecutor::doInference(bool useCudaInputBuffer)
{
    static QElapsedTimer timer;
    timer.restart();
    cudaError_t result;
    /*cudaStream_t cudaStream;
    cudaStreamCreate(&cudaStream);*/

    if (!useCudaInputBuffer)
    {
        float* input = m_hostBuffers[m_input.index];

        if (input != nullptr)
        {
            result = cudaMemcpy(
                m_deviceBuffers[m_input.index],
                input,
                m_input.size,
                cudaMemcpyHostToDevice/*,
                cudaStream*/);

            qDebug() << "=============> COPY TO DEVICE TOOK" << timer.elapsed();
            timer.restart();
        }
    }

    m_executionContext->execute(kBatchSize, (void**)m_deviceBuffers.data()/*, cudaStream, nullptr*/);

    qDebug() << "============> EXECUTION TOOK " << timer.elapsed();
    timer.restart();

    for (const auto& bufferInfo: m_outputs)
    {
        result = cudaMemcpy(
            m_hostBuffers[bufferInfo.index],
            m_deviceBuffers[bufferInfo.index],
            bufferInfo.size,
            cudaMemcpyDeviceToHost/*,
            cudaStream*/);

        qDebug() << "==================> COPY FROM DEVICE TOOK" << timer.elapsed();
    }

    /*cudaStreamSynchronize(cudaStream);
    cudaStreamDestroy(cudaStream);*/
}

void* GieExecutor::getBuffer(int index) const
{
    if (index >= m_hostBuffers.size())
        return nullptr;

    return m_hostBuffers[index];
}

float* GieExecutor::getInputBuffer() const
{
    if (m_input.index >= m_hostBuffers.size())
        return nullptr;

    return m_hostBuffers[m_input.index];
}

Dims3 GieExecutor::getOutputDimensions(int bufferIndex) const
{
    for (const auto& out: m_outputs)
    {
        if (out.index == bufferIndex)
            return out.dimensions;
    }

    return Dims3();
}

int GieExecutor::getNetWidth() const
{
    if (strcmp(conf.netType, "pednet"))
        return kPedNetWidth;

    return kCarNetWidth;
}

int GieExecutor::getNetHeight() const
{
    if (strcmp(conf.netType, "pednet"))
        return kPedNetWidth;

    return kCarNetHeight;
}

int GieExecutor::getChannelNumber() const
{
    return kChannelNumber;
}

void GieExecutor::allocateMemory(bool useCpuBuffers)
{
    const auto& cudaEngine = m_executionContext->getEngine();
    auto numBindings = cudaEngine.getNbBindings();

    NX_ASSERT(
        numBindings == kNumBindings,
        lit("Number of bindings should be equal to %1. Got %2")
            .arg(kNumBindings)
            .arg(numBindings));

    std::size_t bufferCount = m_outputs.size() + 1;
    m_deviceBuffers.resize(bufferCount);
    m_hostBuffers.resize(bufferCount);


    m_input.index = cudaEngine.getBindingIndex(m_input.name.toStdString().c_str());
    m_input.dimensions = cudaEngine.getBindingDimensions(m_input.index);
    m_input.size = kBatchSize
        * m_input.dimensions.c
        * m_input.dimensions.h
        * m_input.dimensions.w
        * sizeof(float);

    if (useCpuBuffers)
        m_hostBuffers[m_input.index] = new float[m_input.size / sizeof(float)];

    auto result = cudaMalloc(&m_deviceBuffers[m_input.index], m_input.size);
    NX_ASSERT(result == ::cudaSuccess, lit("Can't allocate input buffer on the device."));

    for (auto& output: m_outputs)
    {
        output.index = cudaEngine.getBindingIndex(output.name.toStdString().c_str());
        output.dimensions = cudaEngine.getBindingDimensions(output.index);
        output.size = kBatchSize
            * output.dimensions.c
            * output.dimensions.h
            * output.dimensions.w
            * sizeof(float);

        result = cudaMalloc(&m_deviceBuffers[output.index], output.size);
        NX_ASSERT(result == ::cudaSuccess, lit("Can't allocate output buffer on the device."));

        m_hostBuffers[output.index] = new float[output.size / sizeof(float)];
    }
}

void GieExecutor::releaseMemory()
{
    for (auto& buffer: m_deviceBuffers)
    {
        if (buffer != nullptr)
            cudaFree(buffer);

        buffer = nullptr;
    }

    for (auto& buffer: m_hostBuffers)
    {
        if (buffer != nullptr)
            delete [] buffer;

        buffer = nullptr;
    }
}

} // namespace analytics
} // namespace nx

#endif // ENABLE_NVIDIA_ANALYTICS
