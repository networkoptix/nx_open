#include "dualstreaming_helper.h"
#include "core/resource/camera_resource.h"

QnDualStreamingHelper::QnDualStreamingHelper():
    m_lastMotionTime(AV_NOPTS_VALUE)
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        m_motionMaskBinData[i] = (__m128i*) qMallocAligned(MD_WIDTH * MD_HEIGHT/8, 32);
        memset(m_motionMaskBinData[i], 0, MD_WIDTH * MD_HEIGHT/8);
    }
}

QnDualStreamingHelper::~QnDualStreamingHelper()
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) 
        qFreeAligned(m_motionMaskBinData[i]);
}

qint64 QnDualStreamingHelper::getLastMotionTime()
{
    QMutexLocker lock(&m_mutex);
    return m_lastMotionTime;
}

void QnDualStreamingHelper::onMotion(QnMetaDataV1Ptr motion)
{
    QMutexLocker lock(&m_mutex);
    motion->removeMotion(m_motionMaskBinData[motion->channelNumber]);
    if(!motion->isEmpty())
        m_lastMotionTime = motion->timestamp;
    else
        m_lastMotionTime = AV_NOPTS_VALUE;
}

void QnDualStreamingHelper::updateCamera(QnSecurityCamResourcePtr cameraRes)
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        QnMetaDataV1::createMask(cameraRes->getMotionMask(i), (char*)m_motionMaskBinData[i]);
}
