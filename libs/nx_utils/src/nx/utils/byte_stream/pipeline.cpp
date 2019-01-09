#include "pipeline.h"

#include "../random.h"

namespace nx {
namespace utils {
namespace bstream {

//-------------------------------------------------------------------------------------------------
// AbstractOutputConverter

void AbstractOutputConverter::setOutput(AbstractOutput* outputStream)
{
    m_outputStream = outputStream;
}

//-------------------------------------------------------------------------------------------------
// AbstractTwoWayConverter

void AbstractInputConverter::setInput(AbstractInput* inputStream)
{
    m_inputStream = inputStream;
}

//-------------------------------------------------------------------------------------------------

int CompositeConverter::write(const void* data, size_t size)
{
    return m_outputConverter
        ? m_outputConverter->write(data, size)
        : m_outputStream->write(data, size);
}

int CompositeConverter::read(void* data, size_t size)
{
    return m_inputConverter
        ? m_inputConverter->read(data, size)
        : m_inputStream->read(data, size);
}

void CompositeConverter::setOutput(AbstractOutput* output)
{
    base_type::setOutput(output);

    if (m_outputConverter)
        m_outputConverter->setOutput(output);
}

void CompositeConverter::setInput(AbstractInput* input)
{
    base_type::setInput(input);

    if (m_inputConverter)
        m_inputConverter->setInput(input);
}

bool CompositeConverter::eof() const
{
    return false;
}

bool CompositeConverter::failed() const
{
    return false;
}

void CompositeConverter::setInputConverter(AbstractInputConverter* inputConverter)
{
    m_inputConverter = inputConverter;
    m_inputConverter->setInput(m_inputStream);
}

void CompositeConverter::setOutputConverter(AbstractOutputConverter* outputConverter)
{
    m_outputConverter = outputConverter;
    m_outputConverter->setOutput(m_outputStream);
}

//-------------------------------------------------------------------------------------------------

OutputToInputConverterAdapter::OutputToInputConverterAdapter(
    AbstractOutputConverter* converter)
    :
    m_converter(converter)
{
    m_converter->setOutput(this);
}

int OutputToInputConverterAdapter::read(void* data, size_t count)
{
    if (!m_convertedData.empty())
        return readCachedData(data, count);

    auto result = m_inputStream->read(data, count);
    if (result <= 0)
        return result;

    result = m_converter->write(data, result);
    if (result <= 0)
        return result;

    if (m_convertedData.empty())
        return StreamIoError::wouldBlock;

    return readCachedData(data, count);
}

int OutputToInputConverterAdapter::write(const void* data, size_t size)
{
    m_convertedData.append(static_cast<const char*>(data), size);
    return size;
}

int OutputToInputConverterAdapter::readCachedData(void* data, size_t count)
{
    const auto bytesToCopy = std::min(count, m_convertedData.size());
    memcpy(data, m_convertedData.data(), bytesToCopy);
    m_convertedData.erase(0, bytesToCopy);

    return bytesToCopy;
}

//-------------------------------------------------------------------------------------------------
// ProxyConverter

ProxyConverter::ProxyConverter(Converter* delegate):
    m_delegate(nullptr)
{
    setDelegate(delegate);
}

int ProxyConverter::read(void* data, size_t count)
{
    return m_delegate->read(data, count);
}

int ProxyConverter::write(const void* data, size_t count)
{
    return m_delegate->write(data, count);
}

void ProxyConverter::setInput(AbstractInput* input)
{
    base_type::setInput(input);
    m_delegate->setInput(input);
}

void ProxyConverter::setOutput(AbstractOutput* output)
{
    base_type::setOutput(output);
    m_delegate->setOutput(output);
}

bool ProxyConverter::eof() const
{
    return m_delegate->eof();
}

bool ProxyConverter::failed() const
{
    return m_delegate->failed();
}

void ProxyConverter::setDelegate(Converter* delegate)
{
    m_delegate = delegate;
    if (m_delegate)
    {
        m_delegate->setInput(m_inputStream);
        m_delegate->setOutput(m_outputStream);
    }
}

//-------------------------------------------------------------------------------------------------
// Pipe

Pipe::Pipe(QByteArray initialData):
    m_buffer(std::move(initialData)),
    m_totalBytesThrough(0),
    m_maxSize(0),
    m_eof(false)
{
}

int Pipe::write(const void* data, size_t count)
{
    QnMutexLocker lock(&m_mutex);

    if ((m_maxSize > 0) && ((std::size_t)m_buffer.size() >= m_maxSize))
        return StreamIoError::wouldBlock;

    m_buffer.append(static_cast<const char*>(data), count);
    m_totalBytesThrough += count;
    return count;
}

int Pipe::read(void* data, size_t count)
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

QByteArray Pipe::readAll()
{
    QnMutexLocker lock(&m_mutex);
    QByteArray result;
    result.swap(m_buffer);
    return result;
}

void Pipe::setMaxBufferSize(std::size_t maxSize)
{
    m_maxSize = maxSize;
}

std::size_t Pipe::totalBytesThrough() const
{
    QnMutexLocker lock(&m_mutex);
    return m_totalBytesThrough;
}

QByteArray Pipe::internalBuffer() const
{
    QnMutexLocker lock(&m_mutex);
    return m_buffer;
}

void Pipe::writeEof()
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
