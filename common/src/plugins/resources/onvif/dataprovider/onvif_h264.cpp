#include "onvif_h264.h"
#include "utils/network/rtp_stream_parser.h"
#include "core/resource/network_resource.h"
#include "utils/network/h264_rtp_parser.h"


RTPH264StreamreaderDelegate::RTPH264StreamreaderDelegate(QnResourcePtr res, QString request):
QnResourceConsumer(res),
m_streamParser(0),
m_request(request)
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

QnAbstractMediaDataPtr RTPH264StreamreaderDelegate::getNextData()
{

    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if(m_streamParser) {
        QnAbstractMediaDataPtr rez = m_streamParser->getNextData();
        if (rez)
            return rez;
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
    QTextStream(&url) << "rtsp://" << nres->getHostAddress().toString() << "/" << m_request;

    if (m_RtpSession.open(url))
    {
        m_rtpIo = m_RtpSession.play(AV_NOPTS_VALUE, AV_NOPTS_VALUE, 1.0);
        if (m_rtpIo)
        {
            m_streamParser = new CLH264RtpParser(m_rtpIo);
            m_streamParser->setSDPInfo(m_RtpSession.getSdp());
        }
    }

}

void RTPH264StreamreaderDelegate::closeStream()
{
    m_RtpSession.stop();
    delete m_streamParser;
    m_streamParser = 0;
}

bool RTPH264StreamreaderDelegate::isStreamOpened() const
{
    return m_RtpSession.isOpened();
}
