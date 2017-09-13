#include "avi_motion_archive_delegate.h"
#if defined(ENABLE_SOFTWARE_MOTION_DETECTION)

namespace nx {
namespace mediaserver_core {
namespace plugins {

AviMotionArchiveDelegate::AviMotionArchiveDelegate():
    QnAviArchiveDelegate()
{
    m_motionEstimation.setChannelNum(0);
}

QnAbstractMediaDataPtr AviMotionArchiveDelegate::getNextData()
{
    if (m_prevVideoFrame)
    {
        auto result =  m_prevVideoFrame;
        m_prevVideoFrame.reset();
        return result;
    }

    QnMetaDataV1Ptr motion(nullptr);
    auto mediaData = base_type::getNextData();

    if (mediaData && mediaData->dataType == QnAbstractMediaData::DataType::VIDEO)
        motion = analyzeMotion(mediaData);

    if (motion)
    {
        m_prevVideoFrame = mediaData;
        return motion;
    }

    return mediaData;
}

void AviMotionArchiveDelegate::setMotionRegion(const QnMotionRegion& region)
{
    QnMutexLocker lock(&m_mutex);
    m_motionEstimation.setMotionMask(region);
}

QnMetaDataV1Ptr AviMotionArchiveDelegate::analyzeMotion(const QnAbstractMediaDataPtr& media)
{
    QnMutexLocker lock(&m_mutex);
    auto video = std::dynamic_pointer_cast<QnCompressedVideoData>(media);
    if (!video)
        return nullptr;

    bool result = m_motionEstimation.analizeFrame(video);

    if (!result)
        return nullptr;

    if (m_motionEstimation.existsMetadata())
        return std::dynamic_pointer_cast<QnMetaDataV1>(m_motionEstimation.getMotion());

    return nullptr;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_SOFTWARE_MOTION_DETECTION)
