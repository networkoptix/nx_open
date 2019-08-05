#include "abstract_video_decoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <iostream>
#include <memory>

extern "C"
{
    #include <libavcodec/avcodec.h>
}

#include <nx/utils/log/log.h>
#include <common/static_common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource/param.h>
#include <core/resource/security_cam_resource.h>

void QnAbstractVideoDecoder::setTryHardwareAcceleration( bool tryHardwareAcceleration )
{
    m_tryHardwareAcceleration = tryHardwareAcceleration;
}

QnAbstractVideoDecoder::QnAbstractVideoDecoder()
:
    m_tryHardwareAcceleration(false),
    m_hardwareAccelerationEnabled(false)
{
}

bool QnAbstractVideoDecoder::isHardwareAccelerationEnabled() const
{
    return m_hardwareAccelerationEnabled;
}

#endif // ENABLE_DATA_PROVIDERS
