#pragma once

#include <nx/streaming/archive_delegate_wrapper.h>
#include <motion/motion_estimation.h>

namespace nx {
namespace vms::server {
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
} // namespace vms::server
} // namespace nx
