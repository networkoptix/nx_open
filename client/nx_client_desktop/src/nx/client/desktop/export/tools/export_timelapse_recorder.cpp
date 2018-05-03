#include "export_timelapse_recorder.h"

#include <nx/streaming/abstract_stream_data_provider.h>

#include <nx/utils/log/assert.h>

namespace
{

static const qint64 kOutputDeltaUsec = 1000000ll / 30; //< 30 fps

} // namespace

namespace nx {
namespace client {
namespace desktop {

ExportTimelapseRecorder::ExportTimelapseRecorder(
    const QnResourcePtr& resource,
    qint64 timeStepUsec)
    :
    base_type(resource),
    m_timeStepUsec(timeStepUsec)
{

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
    return QnStreamRecorder::saveData(md);
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

} // namespace desktop
} // namespace client
} // namespace nx