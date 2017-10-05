#include "abstract_video_decoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <iostream>
#include <memory>

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include <nx/utils/log/log.h>


void QnAbstractVideoDecoder::setTryHardwareAcceleration( bool tryHardwareAcceleration )
{
    m_tryHardwareAcceleration = tryHardwareAcceleration;
}

QnAbstractVideoDecoder::QnAbstractVideoDecoder()
:
    m_tryHardwareAcceleration(false),
    m_hardwareAccelerationEnabled(false),
    m_mtDecoding(false),
    m_needRecreate(false)
{
}

bool QnAbstractVideoDecoder::isHardwareAccelerationEnabled() const 
{
    return m_hardwareAccelerationEnabled;
}

#endif // ENABLE_DATA_PROVIDERS
