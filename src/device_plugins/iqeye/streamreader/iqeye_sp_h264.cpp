#include "iqeye_sp_h264.h"
#include "device/network_device.h"
#include "network/simple_http_client.h"
#include "data/mediadata.h"
#include "network/h264_rtp_parser.h"


CLIQEyeH264treamreader::CLIQEyeH264treamreader(CLDevice* dev)
:CLServerPushStreamreader(dev),
m_streamParser(0)
{

}

CLIQEyeH264treamreader::~CLIQEyeH264treamreader()
{
    delete m_streamParser;    
}

CLAbstractMediaData* CLIQEyeH264treamreader::getNextData()
{

    if (!isStreamOpened())
        return 0;

    if(m_streamParser)
        return m_streamParser->getNextData();

    return 0;



}

void CLIQEyeH264treamreader::openStream()
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

void CLIQEyeH264treamreader::closeStream()
{
    m_RtpSession.stop();
}

bool CLIQEyeH264treamreader::isStreamOpened() const
{
    return m_RtpSession.isOpened();
}
