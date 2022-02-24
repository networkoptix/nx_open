// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/buffer.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace utils {
namespace bstream {

namespace StreamIoError {

/**
 * Use SystemError::getLastOSErrorCode() to receive error detail.
 */
constexpr int osError = -1;
constexpr int wouldBlock = -2;
constexpr int nonRecoverableError = -3;

} // namespace StreamIoError

/**
 * Interface of class that can be used as an output for some byte stream.
 * E.g., it can write stream to some file or send via network.
 */
class NX_UTILS_API AbstractOutput
{
public:
    virtual ~AbstractOutput() = default;

    /**
     * @return Bytes written. In case of failure one of StreamIoError* values.
     */
    virtual int write(const void* data, size_t count) = 0;
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

//-------------------------------------------------------------------------------------------------
// Converters.

/**
 * Output that converts byte stream and passes it to another output.
 * Can be used to organize chains of converters.
 */
class NX_UTILS_API AbstractOutputConverter:
    public AbstractOutput
{
public:
    virtual void setOutput(AbstractOutput*);

protected:
    AbstractOutput* m_outputStream = nullptr;
};

// TODO: #akolesnikov It makes sense to remove AbstractInputConverter and make Converter implement only
// AbstractOutput.

class NX_UTILS_API AbstractInputConverter:
    public AbstractInput
{
public:
    virtual void setInput(AbstractInput*);

protected:
    AbstractInput* m_inputStream = nullptr;
};

/**
 * Interface of class that allows byte stream in both directions while converting it if needed.
 */
class NX_UTILS_API AbstractTwoWayConverter:
    public AbstractInputConverter,
    public AbstractOutputConverter
{
};

// TODO: #akolesnikov Get rid of this class.
class NX_UTILS_API Converter:
    public AbstractTwoWayConverter
{
public:
    virtual bool eof() const = 0;
    virtual bool failed() const = 0;
};

/**
 * By default, just forwards data without any conversion.
 * Conversion can be enabled by setting converters with
 * CompositeConverter::setInputConverter and CompositeConverter::setOutputConverter.
 */
class NX_UTILS_API CompositeConverter:
    public Converter
{
    using base_type = Converter;

public:
    virtual int write(const void* data, size_t size) override;
    virtual int read(void* data, size_t size) override;

    virtual void setOutput(AbstractOutput*) override;
    virtual void setInput(AbstractInput*) override;

    virtual bool eof() const override;
    virtual bool failed() const override;

    void setInputConverter(AbstractInputConverter* inputConverter);
    void setOutputConverter(AbstractOutputConverter* outputConverter);

private:
    AbstractInputConverter* m_inputConverter = nullptr;
    AbstractOutputConverter* m_outputConverter = nullptr;
};

/**
 * Input stream that transfers all data through a converter.
 */
class NX_UTILS_API OutputConverterToInputAdapter:
    public AbstractInputConverter,
    private AbstractOutput
{
public:
    OutputConverterToInputAdapter(AbstractOutputConverter* converter);

    virtual int read(void* data, size_t count) override;

private:
    AbstractOutputConverter* m_converter = nullptr;
    std::string m_convertedData;

    virtual int write(const void* data, size_t size) override;

    int readCachedData(void* data, size_t count);
};

//-------------------------------------------------------------------------------------------------
// ProxyConverter

/**
 * Just forwards every call to another object.
 */
class NX_UTILS_API ProxyConverter:
    public Converter
{
    using base_type = Converter;

public:
    ProxyConverter(Converter* delegate);

    virtual int read(void* data, size_t count) override;
    virtual int write(const void* data, size_t count) override;

    virtual void setInput(AbstractInput*) override;
    virtual void setOutput(AbstractOutput*) override;

    virtual bool eof() const override;
    virtual bool failed() const override;

    void setDelegate(Converter* delegate);

private:
    Converter* m_delegate;
};

//-------------------------------------------------------------------------------------------------
// Pipe

/**
 * Data that is written to object of this class with AbstractOutput::write,
 * becomes available through AbstractInput::read.
 */
class NX_UTILS_API Pipe:
    public AbstractInput, public AbstractOutput
{
public:
    Pipe(nx::Buffer initialData = nx::Buffer());

    virtual int write(const void* data, size_t count) override;
    /**
     * Reads data that has previously been written with Pipe::write.
     */
    virtual int read(void* data, size_t count) override;
    nx::Buffer readAll();

    /**
     * @param maxSize 0 - no limit.
     */
    void setMaxBufferSize(std::size_t maxSize);

    /**
     * Total of bytes written and read.
     */
    std::size_t totalBytesThrough() const;

    nx::Buffer internalBuffer() const;

    /**
     * Subsequent read operation will report read error after depleting internal buffer.
     */
    void writeEof();

private:
    mutable nx::Mutex m_mutex;
    nx::Buffer m_buffer;
    std::size_t m_totalBytesThrough;
    std::size_t m_maxSize;
    bool m_eof;
};

//-------------------------------------------------------------------------------------------------
// RandomDataSource

class NX_UTILS_API RandomDataSource:
    public AbstractInput
{
public:
    static constexpr std::size_t kDefaultMinReadSize = 4*1024;
    static constexpr std::size_t kDefaultMaxReadSize = 64*1024;

    RandomDataSource();

    virtual int read(void* data, size_t count) override;

private:
    const std::pair<std::size_t, std::size_t> m_readSizeRange;
};

//-------------------------------------------------------------------------------------------------

template<typename WriteFunc>
class CustomOutput:
    public AbstractOutput
{
public:
    template<typename WriteFuncRef>
    CustomOutput(WriteFuncRef&& func):
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
std::unique_ptr<CustomOutput<WriteFunc>> makeCustomOutput(WriteFunc func)
{
    return std::make_unique<CustomOutput<WriteFunc>>(std::move(func));
}

//-------------------------------------------------------------------------------------------------

template<typename WriteFunc>
class CustomInput:
    public AbstractInput
{
public:
    template<typename WriteFuncRef>
    CustomInput(WriteFuncRef&& func):
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
std::unique_ptr<CustomInput<ReadFunc>> makeCustomInput(ReadFunc func)
{
    return std::make_unique<CustomInput<ReadFunc>>(std::move(func));
}

} // namespace bstream
} // namespace utils
} // namespace nx
