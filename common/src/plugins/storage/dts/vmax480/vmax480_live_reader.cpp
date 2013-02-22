#include "vmax480_live_reader.h"
#include "core/resource/network_resource.h"
#include "core/datapacket/media_data_packet.h"
#include "libavcodec/avcodec.h"
#include "utils/common/sleep.h"
#include "utils/common/synctime.h"

#include "plugins/resources/arecontvision/tools/nalconstructor.cpp"
#include "vmax480_tcp_server.h"


static const int PROCESS_TIMEOUT = 1000;

// ----------------------------------- QnVMax480LiveProvider -----------------------

QnVMax480LiveProvider::QnVMax480LiveProvider(QnResourcePtr dev ):
    CLServerPushStreamreader(dev),
    m_connected(false),
    m_internalQueue(16),
    m_vMaxProxy(0)
{
    m_networkRes = dev.dynamicCast<QnNetworkResource>();
}

QnVMax480LiveProvider::~QnVMax480LiveProvider()
{

}

QnAbstractMediaDataPtr QnVMax480LiveProvider::getNextData()
{
    if (!isStreamOpened()) {
        openStream();
        if (!isStreamOpened())
            return QnAbstractMediaDataPtr(0);
    }

    if (needMetaData())
        return getMetaData();

    QnAbstractDataPacketPtr result;
    QTime getTimer;
    getTimer.restart();
    while (!needToStop() && getTimer.elapsed() < MAX_FRAME_DURATION * 2 && !result)
    {
        m_internalQueue.pop(result, 100);
    }

    if (!result)
        closeStream();
    return result.dynamicCast<QnAbstractMediaData>();
}

void QnVMax480LiveProvider::onGotData(QnAbstractMediaDataPtr mediaData)
{
    m_internalQueue.push(mediaData);
}

void QnVMax480LiveProvider::openStream()
{
    if (m_connected)
        return;

    int port = 0;
    for (int i = 0; i < 1000 && port == 0; ++i)
    {
        port = QnVMax480Server::instance()->getPort();
        if (port == 0)
            msleep(1); // waiting for TCP server started
    }

    QStringList args;
    args << QString::number(port);
    m_tcpID = QnVMax480Server::instance()->registerProvider(this);
    args << m_tcpID;
    m_vMaxProxy = new QProcess();
    m_vMaxProxy->start(QLatin1String("vmaxproxy"), args);
    if (m_vMaxProxy->waitForStarted(PROCESS_TIMEOUT))
    {
        bool rez = QnVMax480Server::instance()->waitForConnection(m_tcpID, PROCESS_TIMEOUT);
        if (rez) {
            QnVMax480Server::instance()->vMaxConnect(m_tcpID, m_networkRes->getUrl(), m_networkRes->getAuth());
            m_connected = true;
        }
    }
    else {
        delete m_vMaxProxy;
        m_vMaxProxy = 0;
    }
}

void QnVMax480LiveProvider::closeStream()
{
    if (!m_connected)
        return;


    QnVMax480Server::instance()->vMaxDisconnect(m_tcpID);

    if (m_vMaxProxy)
    {
        m_vMaxProxy->waitForFinished(PROCESS_TIMEOUT);
        delete m_vMaxProxy;
        m_vMaxProxy = 0;
    }
    QnVMax480Server::instance()->unregisterProvider(this);
    m_connected = false;
}

bool QnVMax480LiveProvider::isStreamOpened() const
{
    return m_connected;
}

void QnVMax480LiveProvider::beforeRun()
{
    msleep(300);
}

void QnVMax480LiveProvider::afterRun()
{
    msleep(300);
}

void QnVMax480LiveProvider::updateStreamParamsBasedOnQuality()
{
    if (isRunning())
        pleaseReOpen();
}

void QnVMax480LiveProvider::updateStreamParamsBasedOnFps()
{
    if (isRunning())
        pleaseReOpen();
}
