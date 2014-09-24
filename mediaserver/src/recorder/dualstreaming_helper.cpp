#include "dualstreaming_helper.h"
#include "core/resource/camera_resource.h"

QnDualStreamingHelper::QnDualStreamingHelper():
    m_lastMotionTime(AV_NOPTS_VALUE)
{
}

QnDualStreamingHelper::~QnDualStreamingHelper()
{
}

qint64 QnDualStreamingHelper::getLastMotionTime()
{
    QMutexLocker lock(&m_mutex);
    return m_lastMotionTime;
}

void QnDualStreamingHelper::onMotion(const QnMetaDataV1* motion)
{
    QMutexLocker lock(&m_mutex);
    //motion->removeMotion(m_motionMaskBinData[motion->channelNumber]);
    if(!motion->isEmpty())
        m_lastMotionTime = motion->timestamp;
    else
        m_lastMotionTime = AV_NOPTS_VALUE;
}
