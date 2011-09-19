
#include "device/network_device.h"
#include "network/simple_http_client.h"
#include "data/mediadata.h"
#include "network/h264_rtp_parser.h"
#include "sp_h264.h"


RTPH264Streamreader::RTPH264Streamreader(CLDevice* dev)
:CLServerPushStreamreader(dev),
m_streamParser(0)
{

}

RTPH264Streamreader::~RTPH264Streamreader()
{
    delete m_streamParser;    
}

CLAbstractMediaData* RTPH264Streamreader::getNextData()
{

    if (!isStreamOpened())
        return 0;

    if(m_streamParser)
        return m_streamParser->getNextData();

    return 0;



}

void RTPH264Streamreader::openStream()
{
    if (isStreamOpened())
        return;

    CLNetworkDevice* ndev = static_cast<CLNetworkDevice*>(m_device);

    QString url;
    QTextStream(&url) << "rtsp://" << ndev->getIP().toString();

    if (m_RtpSession.open(url))
    {
        m_rtpIo = m_RtpSession.play();
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
