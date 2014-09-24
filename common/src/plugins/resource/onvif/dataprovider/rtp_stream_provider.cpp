#include "rtp_stream_provider.h"

#ifdef ENABLE_DATA_PROVIDERS

QnRtpStreamReader::QnRtpStreamReader(const QnResourcePtr& res, const QString& request):
    CLServerPushStreamReader(res),
    m_rtpReader(res),
    m_request(request)
{

}

QnRtpStreamReader::~QnRtpStreamReader()
{

}

void QnRtpStreamReader::setRequest(const QString& request)
{
    m_request = request;
}

QnAbstractMediaDataPtr QnRtpStreamReader::getNextData() 
{
    return m_rtpReader.getNextData();
}

CameraDiagnostics::Result QnRtpStreamReader::openStream() 
{
    //m_rtpReader.setRequest("liveVideoTest");
    //m_rtpReader.setRequest("stream1");
    m_rtpReader.setRole(getRole());
    m_rtpReader.setRequest(m_request);

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

#endif // ENABLE_DATA_PROVIDERS
