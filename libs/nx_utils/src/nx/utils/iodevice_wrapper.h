#pragma once

#include <chrono>

#include <QtCore/QIODevice>
#include <nx/utils/move_only_func.h>

namespace nx::utils {

auto measure(auto f, std::chrono::milliseconds* elapsed)
{
    auto start = std::chrono::steady_clock::now();
    auto result = f();
    *elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    return result;
}

/**
 * This class transparently wraps source QIODevice and provides read and write callbacks
 * on each IO operation.
 */

class IoDeviceWrapper: public QIODevice
{
public:
    using Callback = nx::utils::MoveOnlyFunc<void(qint64, std::chrono::milliseconds)>;

    IoDeviceWrapper(std::unique_ptr<QIODevice> source):
        m_source(std::move(source))
    {
        // Add 'Unbuffered' flag to prevent internal QIODevice buffering.
        if (m_source->isOpen())
            QIODevice::open(m_source->openMode() | QIODevice::Unbuffered);
    }

    void setOnReadCallback(Callback callback) { m_readCallback = std::move(callback); }
    void setOnWriteCallback(Callback callback) { m_writeCallback = std::move(callback); }
    void setOnSeekCallback(Callback callback) { m_seekCallback = std::move(callback); }

    virtual bool open(QIODevice::OpenMode mode) override
    {
        QIODevice::open(mode | QIODevice::Unbuffered);
        return m_source->open(mode);
    }
    virtual qint64 size() const override { return m_source->size(); }
    virtual qint64 pos() const override { return m_source->pos(); }
    virtual void close() override { return m_source->close(); }
    virtual bool seek(qint64 pos) override
    {
        std::chrono::milliseconds elapsed;
        const auto result = measure([this, pos]() { return m_source->seek(pos); }, &elapsed);
        if (m_seekCallback)
            m_seekCallback(result, elapsed);
        return result;
    }

    virtual bool atEnd() const override { return m_source->atEnd(); }

    virtual qint64 writeData(const char * data, qint64 len) override
    {
        std::chrono::milliseconds elapsed;
        const auto result = measure([this, data, len]() { return m_source->write(data, len); }, &elapsed);
        if (m_writeCallback)
            m_writeCallback(result, elapsed);
        return result;
    }

    virtual qint64 readData(char * data, qint64 len) override
    {
        std::chrono::milliseconds elapsed;
        const auto result = measure([this, data, len]() { return m_source->read(data, len); }, &elapsed);
        if (m_readCallback)
            m_readCallback(result, elapsed);
        return result;
    }

    virtual qint64 bytesAvailable() const override { return m_source->bytesAvailable(); }
    virtual qint64 bytesToWrite() const override { return m_source->bytesToWrite(); }
    virtual bool canReadLine() const override { return m_source->canReadLine();  }
    virtual bool isSequential() const override { return m_source->isSequential();  }
    virtual bool reset() override { return m_source->reset();  }
    virtual bool waitForBytesWritten(int msecs) override
    {
        return m_source->waitForBytesWritten(msecs);
    }
    virtual bool waitForReadyRead(int msecs) override { return m_source->waitForReadyRead(msecs); }
private:
    std::unique_ptr<QIODevice> m_source = nullptr;
    Callback m_readCallback = nullptr;
    Callback m_writeCallback = nullptr;
    Callback m_seekCallback = nullptr;
};

} //namespace nx::utils
