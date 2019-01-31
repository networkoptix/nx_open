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
    m_hardwareAccelerationEnabled(false),
    m_mtDecoding(false),
    m_needRecreate(false)
{
}

bool QnAbstractVideoDecoder::isHardwareAccelerationEnabled() const
{
    return m_hardwareAccelerationEnabled;
}

DecoderConfig DecoderConfig::fromMediaResource(QnMediaResourcePtr resource)
{
    return fromResource(resource->toResource()->toSharedPointer());
}

DecoderConfig DecoderConfig::fromResource(QnResourcePtr resource)
{
    DecoderConfig config;
    if (auto cameraResource = resource.dynamicCast<QnSecurityCamResource>())
    {
        config.disabledCodecsForMtDecoding = cameraResource->resourceData().value<QList<QString>>(
            ResourceDataKey::kDisableMultiThreadDecoding);
    }
    return config;
}

#endif // ENABLE_DATA_PROVIDERS
