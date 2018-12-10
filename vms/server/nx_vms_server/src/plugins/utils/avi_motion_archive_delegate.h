#pragma once

#include <core/resource/avi/avi_archive_delegate.h>
#include <motion/motion_estimation.h>

namespace nx {
namespace vms::server {
namespace plugins {

class AviMotionArchiveDelegate: public QnAviArchiveDelegate
{
    using base_type = QnAviArchiveDelegate;

public:
    AviMotionArchiveDelegate();
    virtual ~AviMotionArchiveDelegate() override;

    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void setMotionRegion(const QnMotionRegion& region) override;

private:
    QnMetaDataV1Ptr analyzeMotion(const QnAbstractMediaDataPtr& video);

private:
    mutable QnMutex m_mutex;
    QnMotionEstimation m_motionEstimation;
    QnAbstractMediaDataPtr m_prevVideoFrame;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
