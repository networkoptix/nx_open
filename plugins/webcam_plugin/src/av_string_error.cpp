#include "StdAfx.h"

#include "av_string_error.h"

extern "C"
{
#include <libavutil/error.h>
}

#include <QtCore/qdebug.h>


AVStringError::AVStringError()
{
}

AVStringError::~AVStringError()
{
}

void AVStringError::setAvError(const QString& error)
{
    m_lastError = error;
    qDebug() << m_lastError;
}

QString AVStringError::avErrorString()
{
    return m_lastError;
}

bool AVStringError::updateIfError(int errorCode)
{
    bool error = errorCode < 0;
    if (error)
    {
        const int length = 256;
        char errorBuffer[length];
        av_strerror(errorCode, errorBuffer, length);
        setAvError(errorBuffer);
    }
    return error;
}

bool AVStringError::hasError()
{
    return !m_lastError.isEmpty();
}
