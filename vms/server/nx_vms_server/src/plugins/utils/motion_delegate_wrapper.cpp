#include "motion_delegate_wrapper.h"

namespace nx {
namespace vms::server {
namespace plugins {

MotionDelegateWrapper::MotionDelegateWrapper(std::unique_ptr<QnAbstractArchiveDelegate> delegate):
    base_type(std::move(delegate))
{
    m_motionEstimation.setChannelNum(0);
}

QnAbstractMediaDataPtr MotionDelegateWrapper::getNextData()
{
    auto wrappedDelegate = delegate();
    if (m_prevVideoFrame)
    {
        auto result = m_prevVideoFrame;
        m_prevVideoFrame.reset();
        return result;
    }

    QnMetaDataV1Ptr motion(nullptr);
    auto mediaData = wrappedDelegate->getNextData();

    if (mediaData && mediaData->dataType == QnAbstractMediaData::DataType::VIDEO)
        motion = analyzeMotion(mediaData);

    if (motion)
    {
        m_prevVideoFrame = mediaData;
        motion->timestamp = mediaData->timestamp;
        return motion;
    }

    return mediaData;
}

void MotionDelegateWrapper::setMotionRegion(const QnMotionRegion& region)
{
    QnMutexLocker lock(&m_mutex);
    m_motionEstimation.setMotionMask(region);
}

QnMetaDataV1Ptr MotionDelegateWrapper::analyzeMotion(const QnAbstractMediaDataPtr& media)
{
    QnMutexLocker lock(&m_mutex);
    auto video = std::dynamic_pointer_cast<QnCompressedVideoData>(media);
    if (!video)
        return nullptr;

    bool result = m_motionEstimation.analyzeFrame(video);

    if (!result)
        return nullptr;

    if (m_motionEstimation.existsMetadata())
        return std::dynamic_pointer_cast<QnMetaDataV1>(m_motionEstimation.getMotion());

    return nullptr;
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
