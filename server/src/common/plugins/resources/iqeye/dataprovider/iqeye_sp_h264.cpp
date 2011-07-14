#include "iqeye_sp_h264.h"
#include "network/rtp_stream_parser.h"
#include "resource/network_resource.h"
#include "network/h264_rtp_parser.h"


CLIQEyeH264treamreader::CLIQEyeH264treamreader(QnResource* dev)
:QnServerPushDataProvider(dev),
m_streamParser(0)
{

}

CLIQEyeH264treamreader::~CLIQEyeH264treamreader()
{
    delete m_streamParser;    
}

QnAbstractMediaDataPacketPtr CLIQEyeH264treamreader::getNextData()
{

    if (!isStreamOpened())
        return QnAbstractMediaDataPacketPtr(0);

    if(m_streamParser)
        return m_streamParser->getNextData();

    return QnAbstractMediaDataPacketPtr(0);



}

void CLIQEyeH264treamreader::openStream()
{
    if (isStreamOpened())
        return;

    QnNetworkResource* ndev = static_cast<QnNetworkResource*>(m_device);

    QString url;
    QTextStream(&url) << "rtsp://" << ndev->getHostAddress().toString();

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
