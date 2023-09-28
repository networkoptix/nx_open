// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QIODevice>

#include <nx/utils/move_only_func.h>

namespace nx::utils {

/**
 * This class transparently wraps source QIODevice and provides read and write callbacks
 * on each IO operation.
 */

class NX_UTILS_API IoDeviceWrapper: public QIODevice
{
public:
    using Callback = nx::utils::MoveOnlyFunc<void(qint64, std::chrono::milliseconds)>;

    IoDeviceWrapper(std::unique_ptr<QIODevice> source);
    virtual ~IoDeviceWrapper() override;

    void setOnReadCallback(Callback callback) { m_readCallback = std::move(callback); }
    void setOnWriteCallback(Callback callback) { m_writeCallback = std::move(callback); }
    void setOnSeekCallback(Callback callback) { m_seekCallback = std::move(callback); }

    virtual bool open(QIODevice::OpenMode mode) override;
    virtual qint64 size() const override { return m_source->size(); }
    virtual qint64 pos() const override { return m_source->pos(); }
    virtual void close() override { return m_source->close(); }
    virtual bool seek(qint64 pos) override;
    virtual bool atEnd() const override { return m_source->atEnd(); }
    virtual qint64 writeData(const char * data, qint64 len) override;
    virtual qint64 readData(char * data, qint64 len) override;
    virtual qint64 bytesAvailable() const override { return m_source->bytesAvailable(); }
    virtual qint64 bytesToWrite() const override { return m_source->bytesToWrite(); }
    virtual bool canReadLine() const override { return m_source->canReadLine();  }
    virtual bool isSequential() const override { return m_source->isSequential();  }
    virtual bool reset() override { return m_source->reset();  }
    virtual bool waitForBytesWritten(int msecs) override;
    virtual bool waitForReadyRead(int msecs) override { return m_source->waitForReadyRead(msecs); }

    /*
     * Pass QT event to the wrapped QIODevice. QEvent is used to control BufferedFile.
     */
    virtual bool event(QEvent* e) override { return m_source->event(e); }

private:
    std::unique_ptr<QIODevice> m_source = nullptr;
    Callback m_readCallback = nullptr;
    Callback m_writeCallback = nullptr;
    Callback m_seekCallback = nullptr;
};

} //namespace nx::utils
