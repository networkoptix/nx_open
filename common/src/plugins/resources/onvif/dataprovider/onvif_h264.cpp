#include "onvif_h264.h"
#include "utils/network/rtp_stream_parser.h"
#include "core/resource/network_resource.h"
#include "utils/network/h264_rtp_parser.h"


RTPH264Streamreader::RTPH264Streamreader(QnResourcePtr res)
:CLServerPushStreamreader(res),
m_streamParser(0)
{
    QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(res);
    if (netRes)
        m_RtpSession.setTimeout(netRes->getNetworkTimeout());
    else
        m_RtpSession.setTimeout(1000 * 2);
}

RTPH264Streamreader::~RTPH264Streamreader()
{
    stop();

    delete m_streamParser;    
}

QnAbstractMediaDataPtr RTPH264Streamreader::getNextData()
{

    if (!isStreamOpened())
        return QnAbstractMediaDataPtr(0);

    if(m_streamParser)
        return m_streamParser->getNextData();

    return QnAbstractMediaDataPtr(0);



}

void RTPH264Streamreader::openStream()
{
    if (isStreamOpened())
        return;

    QnNetworkResourcePtr nres = getResource().dynamicCast<QnNetworkResource>();

    QString url;
    QTextStream(&url) << "rtsp://" << nres->getHostAddress().toString();

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

void RTPH264Streamreader::closeStream()
{
    m_RtpSession.stop();
}

bool RTPH264Streamreader::isStreamOpened() const
{
    return m_RtpSession.isOpened();
}
