// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_file_processor.h"

#include <fstream>

#include "file.h"

namespace nx::utils::fs {

class AbstractTask
{
public:
    AbstractTask(IQnFile* file): m_file(file) {}
    virtual ~AbstractTask() = default;

    virtual void run() = 0;

protected:
    IQnFile* m_file = nullptr;
};

//-------------------------------------------------------------------------------------------------

class Stat: public AbstractTask
{
public:
    Stat(const std::string& path, StatHandler handler):
        AbstractTask(nullptr),
        m_path(path),
        m_handler(std::move(handler))
    {
    }

    virtual void run() override
    {
        auto info = stat(m_path.c_str());
        if (info)
            m_handler(SystemError::noError, *info);
        else
            m_handler(SystemError::getLastOSErrorCode(), FileStat{});
    }

private:
    const std::string m_path;
    StatHandler m_handler;
};

//-------------------------------------------------------------------------------------------------

class Open: public AbstractTask
{
public:
    Open(
        IQnFile* file,
        const QIODevice::OpenMode& mode,
        unsigned int systemDependentFlags,
        OpenHandler handler)
        :
        AbstractTask(file),
        m_mode(mode),
        m_systemDependentFlags(systemDependentFlags),
        m_handler(std::move(handler))
    {
    }

    virtual void run() override
    {
        if (m_file->open(m_mode, m_systemDependentFlags))
            m_handler(SystemError::noError);
        else
            m_handler(SystemError::getLastOSErrorCode());
    }

private:
    const QIODevice::OpenMode m_mode;
    unsigned int m_systemDependentFlags = 0;
    OpenHandler m_handler;
};

//-------------------------------------------------------------------------------------------------

class Read: public AbstractTask
{
public:
    Read(
        IQnFile* file,
        nx::Buffer* buf,
        IoCompletionHandler handler)
        :
        AbstractTask(file),
        m_buf(buf),
        m_handler(std::move(handler))
    {
    }

    virtual void run() override
    {
        static constexpr int kDefaultReadSize = 4 * 1024;

        if (m_buf->capacity() <= m_buf->size())
            m_buf->reserve(m_buf->size() + kDefaultReadSize);

        auto bufSize = m_buf->size();
        m_buf->resize(m_buf->capacity());

        const auto bytesRead = m_file->read(
            m_buf->data() + bufSize, m_buf->size() - bufSize);
        if (bytesRead < 0)
        {
            m_buf->resize(bufSize);
            m_handler(SystemError::getLastOSErrorCode(), (std::size_t) -1);
        }
        else
        {
            bufSize += bytesRead;
            m_buf->resize(bufSize);
            m_handler(SystemError::noError, (std::size_t) bytesRead);
        }
    }

private:
    nx::Buffer* m_buf = nullptr;
    IoCompletionHandler m_handler;
};

class ReadAll: public AbstractTask
{
public:
    ReadAll(
        IQnFile* file,
        ReadAllHandler handler)
        :
        AbstractTask(file),
        m_handler(std::move(handler))
    {
    }

    virtual void run() override
    {
        auto data = m_file->readAll();
        if (data)
            m_handler(SystemError::noError, std::move(*data));
        else
            m_handler(SystemError::getLastOSErrorCode(), nx::Buffer());
    }

private:
    ReadAllHandler m_handler;
};

//-------------------------------------------------------------------------------------------------

class Write: public AbstractTask
{
public:
    Write(IQnFile* file, const nx::Buffer& data, IoCompletionHandler handler):
        AbstractTask(file),
        m_data(data),
        m_handler(std::move(handler))
    {
    }

    virtual void run() override
    {
        const auto bytesWritten = m_file->write(m_data.data(), m_data.size());

        if (bytesWritten >= 0)
            m_handler(SystemError::noError, bytesWritten);
        else
            m_handler(SystemError::getLastOSErrorCode(), (std::size_t) -1);
    }

private:
    nx::Buffer m_data;
    IoCompletionHandler m_handler;
};

//-------------------------------------------------------------------------------------------------

class Close: public AbstractTask
{
public:
    Close(IQnFile* file, CloseHandler handler):
        AbstractTask(file),
        m_handler(std::move(handler))
    {
    }

    virtual void run() override
    {
        m_file->close();
        m_handler(SystemError::noError);
    }

private:
    OpenHandler m_handler;
};

//-------------------------------------------------------------------------------------------------

FileAsyncIoScheduler::FileAsyncIoScheduler()
{
    m_ioThread = std::thread([this]() { taskProcessingThread(); });
}

FileAsyncIoScheduler::~FileAsyncIoScheduler()
{
    m_tasks.push(nullptr);

    if (m_ioThread.joinable())
        m_ioThread.join();
}

void FileAsyncIoScheduler::stat(const std::string& path, StatHandler handler)
{
    m_tasks.push(std::make_unique<Stat>(path, std::move(handler)));
}

void FileAsyncIoScheduler::open(
    IQnFile* file,
    const QIODevice::OpenMode& mode,
    unsigned int systemDependentFlags,
    OpenHandler handler)
{
    m_tasks.push(std::make_unique<Open>(file, mode, systemDependentFlags, std::move(handler)));
}

void FileAsyncIoScheduler::read(
    IQnFile* file,
    nx::Buffer* buf,
    IoCompletionHandler handler)
{
    m_tasks.push(std::make_unique<Read>(file, buf, std::move(handler)));
}

void FileAsyncIoScheduler::readAll(
    IQnFile* file,
    ReadAllHandler handler)
{
    m_tasks.push(std::make_unique<ReadAll>(file, std::move(handler)));
}

void FileAsyncIoScheduler::write(
    IQnFile* file,
    const nx::Buffer& buffer,
    IoCompletionHandler handler)
{
    m_tasks.push(std::make_unique<Write>(file, buffer, std::move(handler)));
}

void FileAsyncIoScheduler::close(IQnFile* file, CloseHandler handler)
{
    m_tasks.push(std::make_unique<Close>(file, std::move(handler)));
}

void FileAsyncIoScheduler::taskProcessingThread()
{
    for (;;)
    {
        auto task = m_tasks.pop();
        if (!task)
            break;

        task->run();
    }
}

} // namespace nx::utils::fs
