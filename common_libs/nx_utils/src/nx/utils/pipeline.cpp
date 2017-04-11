#include "pipeline.h"

#include "random.h"

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

ReflectingPipeline::ReflectingPipeline(QByteArray initialData):
    m_buffer(std::move(initialData)),
    m_totalBytesThrough(0),
    m_maxSize(0),
    m_eof(false)
{
}

int ReflectingPipeline::write(const void* data, size_t count)
{
    QnMutexLocker lock(&m_mutex);

    if ((m_maxSize > 0) && ((std::size_t)m_buffer.size() >= m_maxSize))
        return StreamIoError::wouldBlock;

    m_buffer.append(static_cast<const char*>(data), count);
    m_totalBytesThrough += count;
    return count;
}

int ReflectingPipeline::read(void* data, size_t count)
{
    QnMutexLocker lock(&m_mutex);

    if (m_buffer.isEmpty())
        return m_eof ? StreamIoError::osError : StreamIoError::wouldBlock;

    const auto bytesToRead = std::min<size_t>(count, m_buffer.size());
    memcpy(data, m_buffer.data(), bytesToRead);
    m_buffer.remove(0, bytesToRead);
    m_totalBytesThrough += bytesToRead;
    return bytesToRead;
}

void ReflectingPipeline::setMaxBufferSize(std::size_t maxSize)
{
    m_maxSize = maxSize;
}

std::size_t ReflectingPipeline::totalBytesThrough() const
{
    QnMutexLocker lock(&m_mutex);
    return m_totalBytesThrough;
}

QByteArray ReflectingPipeline::internalBuffer() const
{
    QnMutexLocker lock(&m_mutex);
    return m_buffer;
}

void ReflectingPipeline::writeEof()
{
    QnMutexLocker lock(&m_mutex);
    m_eof = true;
}

//-------------------------------------------------------------------------------------------------
// RandomDataSource

constexpr std::size_t RandomDataSource::kDefaultMinReadSize;
constexpr std::size_t RandomDataSource::kDefaultMaxReadSize;

RandomDataSource::RandomDataSource():
    m_readSizeRange(kDefaultMinReadSize, kDefaultMaxReadSize)
{
}

int RandomDataSource::read(void* data, size_t count)
{
    const std::size_t bytesToRead =
        std::min<std::size_t>(
            count,
            random::number<std::size_t>(m_readSizeRange.first, m_readSizeRange.second));
    char* charData = static_cast<char*>(data);
    std::generate(charData, charData + bytesToRead, rand);
    return bytesToRead;
}

} // namespace pipeline
} // namespace utils
} // namespace nx
