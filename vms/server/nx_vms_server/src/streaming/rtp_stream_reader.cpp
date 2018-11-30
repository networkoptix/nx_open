#include "rtp_stream_reader.h"
#if defined(ENABLE_DATA_PROVIDERS)

#include <nx/vms/server/resource/camera.h>

#include <nx/utils/log/log.h>

static const size_t kPacketCountToOmitLog = 50;

QnRtpStreamReader::QnRtpStreamReader(
    const nx::vms::server::resource::CameraPtr& res,
    const QString& request)
    :
    CLServerPushStreamReader(res),
    m_rtpReader(res, res->getTimeOffset()),
    m_request(request),
    m_rtpTransport(RtpTransport::_auto),
    m_camera(res)
{
}

QnRtpStreamReader::~QnRtpStreamReader()
{
    stop();
}

void QnRtpStreamReader::setRtpTransport(const RtpTransport::Value& transport)
{
    m_rtpTransport = transport;
}

void QnRtpStreamReader::setRequest(const QString& request)
{
    m_request = request;
}

QnAbstractMediaDataPtr QnRtpStreamReader::getNextData()
{
    if (!isStreamOpened())
    {
        NX_VERBOSE(this, lm("Next data: stream %1 is closed")
            .args(m_rtpReader.getCurrentStreamUrl()));
        return QnAbstractMediaDataPtr(0);
    }

    if (needMetadata())
    {
        const auto metaData = getMetadata();
        NX_VERBOSE(this, lm("Next data: meta at %1 from %2")
            .args(metaData->timestamp, m_rtpReader.getCurrentStreamUrl()));
        return metaData;
    }

    QnAbstractMediaDataPtr result;
    for (int i = 0; i < 2 && !result; ++i)
        result = m_rtpReader.getNextData();

    if (!result)
    {
        NX_VERBOSE(this, lm("Next data: end of stream %1")
            .args(m_rtpReader.getCurrentStreamUrl()));
        closeStream();
    }
    else
    {
        if (m_dataPassed++ % kPacketCountToOmitLog == 0)
        {
            NX_VERBOSE(this, lm("Next data: media timestamp %1 from %2")
                .args(result->timestamp, m_rtpReader.getCurrentStreamUrl()));
        }
    }

    return result;
}

CameraDiagnostics::Result QnRtpStreamReader::openStreamInternal(bool /*isCameraControlRequired*/, const QnLiveStreamParams& /*params*/)
{
    m_rtpReader.setRole(getRole());
    m_rtpReader.setRequest(m_request);
    m_rtpReader.setRtpTransport(m_rtpTransport);

	m_camera->updateSourceUrl(m_rtpReader.getCurrentStreamUrl(), getRole());

    const auto result = m_rtpReader.openStream();
    NX_VERBOSE(this, lm("Role %1, open stream %2 -> %3")
        .args((int) getRole(), m_rtpReader.getCurrentStreamUrl(), result.toString(nullptr)));

    return result;
}

void QnRtpStreamReader::closeStream()
{
    m_dataPassed = 0;
    m_rtpReader.closeStream();
}

bool QnRtpStreamReader::isStreamOpened() const
{
    return m_rtpReader.isStreamOpened();
}

QnConstResourceAudioLayoutPtr QnRtpStreamReader::getDPAudioLayout() const
{
    return m_rtpReader.getAudioLayout();
}

void QnRtpStreamReader::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    m_rtpReader.pleaseStop();
}

#endif // defined(ENABLE_DATA_PROVIDERS)
