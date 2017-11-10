#include "rtp_stream_provider.h"
#include <core/resource/camera_resource.h>

#ifdef ENABLE_DATA_PROVIDERS

QnRtpStreamReader::QnRtpStreamReader(const QnResourcePtr& res, const QString& request):
    CLServerPushStreamReader(res),
    m_rtpReader(res),
    m_request(request),
    m_rtpTransport(RtpTransport::_auto)
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
        return QnAbstractMediaDataPtr(0);

    if (needMetaData())
        return getMetaData();

    QnAbstractMediaDataPtr result;
    for (int i = 0; i < 2 && !result; ++i)
        result = m_rtpReader.getNextData();

    if (!result)
        closeStream();

    return result;
}

CameraDiagnostics::Result QnRtpStreamReader::openStreamInternal(bool isCameraControlRequired, const QnLiveStreamParams& params)
{
    Q_UNUSED(isCameraControlRequired);
    Q_UNUSED(params);

    m_rtpReader.setRole(getRole());
    m_rtpReader.setRequest(m_request);
    m_rtpReader.setRtpTransport(m_rtpTransport);

	auto virtRes = m_resource.dynamicCast<QnVirtualCameraResource>();

	if (virtRes)
		virtRes->updateSourceUrl(m_rtpReader.getCurrentStreamUrl(), getRole());

    return m_rtpReader.openStream();
}

void QnRtpStreamReader::closeStream()
{
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

#endif // ENABLE_DATA_PROVIDERS
