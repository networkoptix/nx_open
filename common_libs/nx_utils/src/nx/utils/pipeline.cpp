#include "pipeline.h"

namespace nx {
namespace utils {
namespace pipeline {

//-------------------------------------------------------------------------------------------------
// AbstractOutputConverter

AbstractOutputConverter::AbstractOutputConverter():
    m_outputStream(nullptr)
{
}

void AbstractOutputConverter::setOutput(AbstractOutput* outputStream)
{
    m_outputStream = outputStream;
}

//-------------------------------------------------------------------------------------------------
// TwoWayPipeline

TwoWayPipeline::TwoWayPipeline():
    m_inputStream(nullptr)
{
}

void TwoWayPipeline::setInput(AbstractInput* inputStream)
{
    m_inputStream = inputStream;
}

//-------------------------------------------------------------------------------------------------
// ProxyPipeline

int ProxyPipeline::read(void* data, size_t count)
{
    return m_inputStream->read(data, count);
}

int ProxyPipeline::write(const void* data, size_t count)
{
    return m_outputStream->write(data, count);
}

//-------------------------------------------------------------------------------------------------
// ReflectingPipeline

ReflectingPipeline::ReflectingPipeline():
    m_totalBytesThrough(0)
{
}

int ReflectingPipeline::write(const void* data, size_t count)
{
    m_buffer.append(static_cast<const char*>(data), count);
    m_totalBytesThrough += count;
    return count;
}

int ReflectingPipeline::read(void* data, size_t count)
{
    if (m_buffer.isEmpty())
        return StreamIoError::wouldBlock;

    const auto bytesToRead = std::min<size_t>(count, m_buffer.size());
    memcpy(data, m_buffer.data(), bytesToRead);
    m_buffer.remove(0, bytesToRead);
    m_totalBytesThrough += bytesToRead;
    return bytesToRead;
}

std::size_t ReflectingPipeline::totalBytesThrough() const
{
    return m_totalBytesThrough;
}

const QByteArray& ReflectingPipeline::internalBuffer() const
{
    return m_buffer;
}

} // namespace pipeline
} // namespace utils
} // namespace nx
