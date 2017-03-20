#pragma once

#include <memory>

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace utils {
namespace pipeline {

namespace StreamIoError {

/**
 * Use SystemError::getLastOSErrorCode() to receive error detail.
 */
constexpr int osError = -1;
constexpr int wouldBlock = -2;
constexpr int nonRecoverableError = -3;

} // namespace StreamIoError

class NX_UTILS_API AbstractOutput
{
public:
    virtual ~AbstractOutput() = default;

    /**
     * @return Bytes written. In case of failure one of StreamIoError* values.
     */
    virtual int write(const void* data, size_t count) = 0;
};

class NX_UTILS_API AbstractOutputConverter:
    public AbstractOutput
{
public:
    AbstractOutputConverter();

    virtual void setOutput(AbstractOutput*);

protected:
    AbstractOutput* m_outputStream;
};

class NX_UTILS_API AbstractInput
{
public:
    virtual ~AbstractInput() = default;

    /**
     * @return Bytes read. In case of failure one of StreamIoError* values.
     */
    virtual int read(void* data, size_t count) = 0;
};

class NX_UTILS_API TwoWayPipeline:
    public AbstractInput,
    public AbstractOutputConverter
{
public:
    TwoWayPipeline();

    virtual void setInput(AbstractInput*);

protected:
    AbstractInput* m_inputStream;
};

//-------------------------------------------------------------------------------------------------
// ProxyPipeline

class NX_UTILS_API ProxyPipeline:
    public TwoWayPipeline
{
public:
    virtual int read(void* data, size_t count) override;
    virtual int write(const void* data, size_t count) override;
};

//-------------------------------------------------------------------------------------------------
// ReflectingPipeline

class NX_UTILS_API ReflectingPipeline:
    public TwoWayPipeline
{
public:
    ReflectingPipeline();

    virtual int write(const void* data, size_t count) override;
    /**
     * Reads data that has previously been written with ReflectingPipeline::write.
     */
    virtual int read(void* data, size_t count) override;

    /**
     * Total of bytes written and read.
     */
    std::size_t totalBytesThrough() const;

    const QByteArray& internalBuffer() const;

private:
    QByteArray m_buffer;
    std::size_t m_totalBytesThrough;
};

//-------------------------------------------------------------------------------------------------
// Pipeline with template functor.

template<typename WriteFunc>
class CustomOutputPipeline:
    public AbstractOutput
{
public:
    template<typename WriteFuncRef>
    CustomOutputPipeline(WriteFuncRef&& func):
        m_func(std::forward<WriteFuncRef>(func))
    {
    }

    virtual int write(const void* data, size_t count) override
    {
        return m_func(data, count);
    }

private:
    WriteFunc m_func;
};

template<typename WriteFunc>
std::unique_ptr<CustomOutputPipeline<WriteFunc>>
    makeCustomOutputPipeline(WriteFunc func)
{
    return std::make_unique<CustomOutputPipeline<WriteFunc>>(std::move(func));
}


template<typename WriteFunc>
class CustomInputPipeline:
    public AbstractInput
{
public:
    template<typename WriteFuncRef>
    CustomInputPipeline(WriteFuncRef&& func):
        m_func(std::forward<WriteFuncRef>(func))
    {
    }

    virtual int read(void* data, size_t count) override
    {
        return m_func(data, count);
    }

private:
    WriteFunc m_func;
};

template<typename ReadFunc>
std::unique_ptr<CustomInputPipeline<ReadFunc>>
    makeCustomInputPipeline(ReadFunc func)
{
    return std::make_unique<CustomInputPipeline<ReadFunc>>(std::move(func));
}

} // namespace pipeline
} // namespace utils
} // namespace nx
