#include "av_string_error.h"

extern "C" {
#include <libavutil/error.h>
} // extern "C"

AVStringError::AVStringError () : m_errorCode(0)
{
}

void AVStringError::setAvError(const QString& error)
{
    m_lastError = error;
}

QString AVStringError::avErrorString() const
{
    return m_lastError;
}

void AVStringError::setAvErrorCode (int errorCode)
{
    m_errorCode = errorCode;
    updateIfError(errorCode);
}

int AVStringError::errorCode () const
{
    return m_errorCode;
}

bool AVStringError::updateIfError(int errorCode)
{
    bool error = errorCode < 0;
    if (error)
    {
        m_errorCode = errorCode;
        const int length = 256;
        char errorBuffer[length];
        av_strerror(errorCode, errorBuffer, length);
        setAvError(errorBuffer);
    }
    return error;
}

bool AVStringError::hasError() const
{
    return !m_lastError.isEmpty()
    || m_errorCode != 0;
}
