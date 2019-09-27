#pragma once

#include <QtCore/QIODevice>
#include <nx/utils/move_only_func.h>

namespace nx::utils {

/**
 * This class transparently wraps source QIODevice and provides read and write callbacks
 * on each IO operation.
 */

class IoDeviceWrapper: public QIODevice
{
public:
    using Callback = nx::utils::MoveOnlyFunc<void(qint64)>;

    IoDeviceWrapper(std::unique_ptr<QIODevice> source):
        m_source(std::move(source))
    {
        // Add 'Unbuffered' flag to prevent internal QIODevice buffering.
        if (m_source->isOpen())
            QIODevice::open(m_source->openMode() | QIODevice::Unbuffered);
    }

    void setOnReadCallback(Callback callback) { m_readCallback = std::move(callback); }
    void setOnWriteCallback(Callback callback) { m_writeCallback = std::move(callback); }

    virtual bool open(QIODevice::OpenMode mode) override
    {
        QIODevice::open(mode | QIODevice::Unbuffered);
        return m_source->open(mode);
    }
    virtual qint64 size() const override { return m_source->size(); }
    virtual qint64 pos() const override { return m_source->pos(); }
    virtual void close() override { return m_source->close(); }
    virtual bool seek(qint64 pos) override { return m_source->seek(pos); }
    virtual bool atEnd() const override { return m_source->atEnd(); }

    virtual qint64 writeData(const char * data, qint64 len) override
    {
        auto result = m_source->write(data, len);
        if (m_writeCallback)
            m_writeCallback(result);
        return result;
    }
    virtual qint64 readData(char * data, qint64 len) override
    {
        auto result = m_source->read(data, len);
        if (m_readCallback)
            m_readCallback(result);
        return result;
    }
private:
    std::unique_ptr<QIODevice> m_source = nullptr;
    Callback m_readCallback = nullptr;
    Callback m_writeCallback = nullptr;
};

} //namespace nx::utils
