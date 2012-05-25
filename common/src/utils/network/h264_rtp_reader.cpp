#include "utils/network/rtp_stream_parser.h"
#include "core/resource/network_resource.h"
#include "utils/network/h264_rtp_parser.h"
#include "h264_rtp_reader.h"


static const int RTSP_RETRY_COUNT = 3;

RTPH264StreamreaderDelegate::RTPH264StreamreaderDelegate(QnResourcePtr res):
QnResourceConsumer(res),
m_streamParser(0)
{
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(res);
    if (netRes)
        m_RtpSession.setTCPTimeout(netRes->getNetworkTimeout());
    else
        m_RtpSession.setTCPTimeout(1000 * 2);
}

RTPH264StreamreaderDelegate::~RTPH264StreamreaderDelegate()
{
    delete m_streamParser;    
}

void RTPH264StreamreaderDelegate::setRequest(const QString& request)
{
    m_request = request;
}

QnAbstractMediaDataPtr RTPH264StreamreaderDelegate::getNextData()
{
    QnAbstractMediaDataPtr result;
    quint8 rtpBuffer[MAX_RTP_PACKET_SIZE];
    int readed;

    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if(m_streamParser) 
    {
        for (int i = 0; i < RTSP_RETRY_COUNT; ++i)
        {
            do {
                readed = m_rtpIo->read( (char*) rtpBuffer, sizeof(rtpBuffer));
            } while(m_streamParser->processData(rtpBuffer, readed, result ) && !result);
            if (result)
                return result;
        }
    }
    closeStream();
    return QnAbstractMediaDataPtr(0);
}

void RTPH264StreamreaderDelegate::openStream()
{
    if (isStreamOpened())
        return;

    QnNetworkResourcePtr nres = getResource().dynamicCast<QnNetworkResource>();

    QString url;
    if (m_request.length() > 0)
        QTextStream(&url) << "rtsp://" << nres->getHostAddress().toString() << "/" << m_request;
    else
        QTextStream(&url) << "rtsp://" << nres->getHostAddress().toString();

    m_RtpSession.setAuth(nres->getAuth());

    if (m_RtpSession.open(url))
    {
        m_RtpSession.play(AV_NOPTS_VALUE, AV_NOPTS_VALUE, 1.0);
        m_rtpIo = m_RtpSession.getTrackIoByType("video");
        if (m_rtpIo)
        {
            m_streamParser = new CLH264RtpParser();
            m_streamParser->setSDPInfo(m_RtpSession.getSdpByType("video"));
        }
    }

}

void RTPH264StreamreaderDelegate::closeStream()
{
    m_RtpSession.sendTeardown();
    m_RtpSession.stop();
    delete m_streamParser;
    m_streamParser = 0;
}

bool RTPH264StreamreaderDelegate::isStreamOpened() const
{
    return m_RtpSession.isOpened();
}
