// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_timelapse_recorder.h"

#include <nx/streaming/abstract_stream_data_provider.h>

#include <nx/utils/log/assert.h>

namespace
{

static const int kFramerate = 30;
static const qint64 kOutputDeltaUsec = 1000000ll / kFramerate; //< 30 fps

} // namespace

namespace nx::vms::client::desktop {

ExportTimelapseRecorder::ExportTimelapseRecorder(
    const QnResourcePtr& resource,
    QnAbstractMediaStreamDataProvider* mediaProvider,
    qint64 timeStepUsec)
    :
    base_type(resource, mediaProvider),
    m_timeStepUsec(timeStepUsec)
{
    setTranscoderQuality(Qn::StreamQuality::rapidReview);
    setTranscoderFixedFrameRate(kFramerate);
}

ExportTimelapseRecorder::~ExportTimelapseRecorder()
{
    stop();
}

bool ExportTimelapseRecorder::saveData(const QnConstAbstractMediaDataPtr& md)
{
    auto nonConstMd = const_cast<QnAbstractMediaData*> (md.get());
    // we can use non const object if only 1 consumer
    NX_ASSERT(nonConstMd->dataProvider->processorsCount() <= 1);

    if (m_currentAbsoluteTimeUsec < md->timestamp)
        m_currentAbsoluteTimeUsec = md->timestamp;
    else
        m_currentAbsoluteTimeUsec += m_timeStepUsec;

    nonConstMd->timestamp = m_currentAbsoluteTimeUsec;

    return base_type::saveData(md);
}

qint64 ExportTimelapseRecorder::getPacketTimeUsec(const QnConstAbstractMediaDataPtr& /*md*/)
{
    const auto result = m_currentRelativeTimeUsec;
    m_currentRelativeTimeUsec += kOutputDeltaUsec;
    return result;
}

bool ExportTimelapseRecorder::isUtcOffsetAllowed() const
{
    return false;
}

} // namespace nx::vms::client::desktop
