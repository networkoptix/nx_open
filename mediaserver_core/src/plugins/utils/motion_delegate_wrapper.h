#pragma once

#if defined(ENABLE_SOFTWARE_MOTION_DETECTION)

#include <plugins/utils/archive_delegate_wrapper.h>
#include <motion/motion_estimation.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class MotionDelegateWrapper: public ArchiveDelegateWrapper
{
    using base_type = ArchiveDelegateWrapper;
public:
    MotionDelegateWrapper(std::unique_ptr<QnAbstractArchiveDelegate> delegate);

    virtual QnAbstractMediaDataPtr getNextData() override;

    virtual void setMotionRegion(const QnMotionRegion& region) override;

private:
    QnMetaDataV1Ptr analyzeMotion(const QnAbstractMediaDataPtr& media);

private:
    mutable QnMutex m_mutex;
    QnMotionEstimation m_motionEstimation;
    QnAbstractMediaDataPtr m_prevVideoFrame;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // ENABLE_SOFTWARE_MOTION
