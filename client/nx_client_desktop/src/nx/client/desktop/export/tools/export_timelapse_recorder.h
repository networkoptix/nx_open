#pragma once

#include <core/resource/resource_fwd.h>

#include <recording/stream_recorder.h>

namespace nx {
namespace client {
namespace desktop {

// input video with steps between frames in timeStepUsec translated to output video with 30 fps (kOutputDeltaUsec between frames)
class ExportTimelapseRecorder: public QnStreamRecorder
{
    using base_type = QnStreamRecorder;

public:
    ExportTimelapseRecorder(const QnResourcePtr& resource, qint64 timeStepUsec);
    virtual ~ExportTimelapseRecorder() override;

protected:
    virtual bool saveData(const QnConstAbstractMediaDataPtr& md) override;
    virtual qint64 getPacketTimeUsec(const QnConstAbstractMediaDataPtr& md) override;
    virtual bool isUtcOffsetAllowed() const override;

private:
    const qint64 m_timeStepUsec;
    qint64 m_currentRelativeTimeUsec = 0;
    qint64 m_currentAbsoluteTimeUsec = std::numeric_limits<qint64>::min();
};

} // namespace desktop
} // namespace client
} // namespace nx
