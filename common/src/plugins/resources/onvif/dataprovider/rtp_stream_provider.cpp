#include "rtp_stream_provider.h"

QnRtpStreamReader::QnRtpStreamReader(QnResourcePtr res, const QString& request):
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

CameraDiagnostics::ErrorCode::Value QnRtpStreamReader::openStream() 
{
    //m_rtpReader.setRequest("liveVideoTest");
    //m_rtpReader.setRequest("stream1");
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

const QnResourceAudioLayout* QnRtpStreamReader::getDPAudioLayout() const
{
    return m_rtpReader.getAudioLayout();
}
