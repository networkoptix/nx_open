#include "iqeye_sp_h264.h"
#include "device/network_device.h"
#include "network/simple_http_client.h"
#include "data/mediadata.h"


CLIQEyeH264treamreader::CLIQEyeH264treamreader(CLDevice* dev)
:CLServerPushStreamreader(dev)

{

}

CLIQEyeH264treamreader::~CLIQEyeH264treamreader()
{
    
}

CLAbstractMediaData* CLIQEyeH264treamreader::getNextData()
{

    if (!isStreamOpened())
        return 0;

    while(1)
    {
        if (m_rtpIo)
            m_rtpIo->read(1024);
    }

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
        m_rtpIo = m_RtpSession.play();

}

void CLIQEyeH264treamreader::closeStream()
{
    m_RtpSession.stop();
}

bool CLIQEyeH264treamreader::isStreamOpened() const
{
    return m_RtpSession.isOpened();
}
