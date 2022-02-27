// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/vms/client/desktop/export/tools/export_storage_stream_recorder.h>

namespace nx::vms::client::desktop {

// input video with steps between frames in timeStepUsec translated to output video with 30 fps (kOutputDeltaUsec between frames)
class ExportTimelapseRecorder: public ExportStorageStreamRecorder
{
    using base_type = ExportStorageStreamRecorder;

public:
    ExportTimelapseRecorder(
        const QnResourcePtr& resource,
        QnAbstractMediaStreamDataProvider* mediaProvider,
        qint64 timeStepUsec);
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

} // namespace nx::vms::client::desktop
