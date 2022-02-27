// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "iodevice_wrapper.h"

#include <nx/utils/elapsed_timer.h>

namespace nx::utils {

IoDeviceWrapper::IoDeviceWrapper(std::unique_ptr<QIODevice> source):
    m_source(std::move(source))
{
    // Add 'Unbuffered' flag to prevent internal QIODevice buffering.
    if (m_source->isOpen())
        QIODevice::open(m_source->openMode() | QIODevice::Unbuffered);
}

IoDeviceWrapper::~IoDeviceWrapper()
{
    ElapsedTimer timer(ElapsedTimerState::started);
    m_source.reset(); //< It could flash buffer on reset.
    if (m_writeCallback)
        m_writeCallback(0, timer.elapsed());
}

bool IoDeviceWrapper::open(QIODevice::OpenMode mode)
{
    QIODevice::open(mode | QIODevice::Unbuffered);
    return m_source->open(mode);
}

bool IoDeviceWrapper::seek(qint64 pos)
{
    ElapsedTimer timer(ElapsedTimerState::started);
    const auto result = m_source->seek(pos);
    if (m_seekCallback)
        m_seekCallback(result, timer.elapsed());
    return result;
}

qint64 IoDeviceWrapper::writeData(const char * data, qint64 len)
{
    ElapsedTimer timer(ElapsedTimerState::started);
    const auto result = m_source->write(data, len);
    if (m_writeCallback)
        m_writeCallback(result, timer.elapsed());
    return result;
}

qint64 IoDeviceWrapper::readData(char * data, qint64 len)
{
    ElapsedTimer timer(ElapsedTimerState::started);
    const auto result = m_source->read(data, len);
    if (m_readCallback)
        m_readCallback(result, timer.elapsed());
    return result;
}

bool IoDeviceWrapper::waitForBytesWritten(int msecs)
{
    return m_source->waitForBytesWritten(msecs);
}
} // namespace nx::utils
