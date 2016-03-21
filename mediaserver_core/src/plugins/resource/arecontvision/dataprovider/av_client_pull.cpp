#ifdef ENABLE_ARECONT

#include "av_client_pull.h"

#include <utils/common/log.h>
#include <utils/common/util.h>

#include "../resource/av_resource.h"


QnPlAVClinetPullStreamReader::QnPlAVClinetPullStreamReader(const QnResourcePtr& res)
:
    parent_type(res),
    m_videoFrameBuff(CL_MEDIA_ALIGNMENT, 1024*1024)
{
    /**/
    //setQuality(getQuality());  // to update stream params

    QnPlAreconVisionResourcePtr avRes = res.dynamicCast<QnPlAreconVisionResource>();
    m_panoramic = avRes->isPanoramic();
    m_dualsensor = avRes->isDualSensor();
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
