#ifdef ENABLE_ARECONT

#include "av_client_pull.h"

#include <nx/utils/log/log.h>
#include <utils/common/util.h>

#include "../resource/av_resource.h"
#include <nx/streaming/config.h>

QnPlAVClinetPullStreamReader::QnPlAVClinetPullStreamReader(const QnPlAreconVisionResourcePtr& res)
:
    parent_type(res),
    m_videoFrameBuff(CL_MEDIA_ALIGNMENT, 1024*1024)
{
    /**/
    //setQuality(getQuality());  // to update stream params

    m_panoramic = m_camera->isPanoramic();
    m_dualsensor = m_camera->isDualSensor();
}

QnPlAVClinetPullStreamReader::~QnPlAVClinetPullStreamReader()
{
    stop();
}

int QnPlAVClinetPullStreamReader::getBitrateMbps() const
{
    return getResource()->getProperty(lit("Bitrate")).toInt();
}

bool QnPlAVClinetPullStreamReader::isH264() const
{
    return getResource()->getProperty(lit("Codec")) == lit("H.264");
}

bool QnPlAVClinetPullStreamReader::isCameraControlRequired() const
{
    return true;
}

#endif
