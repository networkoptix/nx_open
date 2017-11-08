#pragma once
#if defined(ENABLE_SOFTWARE_MOTION_DETECTION)

#include <core/resource/avi/avi_archive_delegate.h>
#include <motion/motion_estimation.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class AviMotionArchiveDelegate: public QnAviArchiveDelegate
{
    using base_type = QnAviArchiveDelegate;

public:
    AviMotionArchiveDelegate();

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
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_SOFTWARE_MOTION_DETECTION)
