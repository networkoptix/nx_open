#include "vmax480_stream_fetcher.h"
#include "vmax480_tcp_server.h"
#include "utils/common/sleep.h"
#include "core/resource/network_resource.h"
#include "vmax480_resource.h"

static const int PROCESS_TIMEOUT = 1000;

VMaxStreamFetcher::VMaxStreamFetcher(QnResourcePtr dev):
    m_vMaxProxy(0)
{
    m_res = dev.dynamicCast<QnNetworkResource>();
}

VMaxStreamFetcher::~VMaxStreamFetcher()
{
    //vmaxDisconnect();
}

int VMaxStreamFetcher::getPort()
{
    int port = 0;
    for (int i = 0; i < 1000 && port == 0; ++i)
    {
        port = QnVMax480Server::instance()->getPort();
        if (port == 0)
            QnSleep::msleep(1); // waiting for TCP server started
    }
    return port;
}

bool VMaxStreamFetcher::isOpened() const
{
    return m_vMaxProxy && !m_tcpID.isEmpty();
}

void VMaxStreamFetcher::vmaxArchivePlay(qint64 timeUsec, quint8 sequence)
{
    QnVMax480Server::instance()->vMaxArchivePlay(m_tcpID, timeUsec, sequence);
}

bool VMaxStreamFetcher::vmaxConnect(bool isLive)
{
    int port = getPort();
    QStringList args;
    args << QString::number(port);
    m_tcpID = QnVMax480Server::instance()->registerProvider(this);
    args << m_tcpID;
    m_vMaxProxy = new QProcess();
    m_vMaxProxy->startDetached(QLatin1String("vmaxproxy"), args);
    if (m_vMaxProxy->waitForStarted(PROCESS_TIMEOUT) || true)
    {
        bool rez = QnVMax480Server::instance()->waitForConnection(m_tcpID, PROCESS_TIMEOUT);
        if (rez) {
            QnVMax480Server::instance()->vMaxConnect(m_tcpID, m_res->getUrl(), m_res->getAuth(), isLive);
            return true;
        }
    }
    else {
        delete m_vMaxProxy;
        m_vMaxProxy = 0;
    }

    return false;
}

void VMaxStreamFetcher::vmaxDisconnect()
{
    QnVMax480Server::instance()->vMaxDisconnect(m_tcpID);

    if (m_vMaxProxy)
    {
        m_vMaxProxy->waitForFinished(PROCESS_TIMEOUT);
        delete m_vMaxProxy;
        m_vMaxProxy = 0;
    }
    QnVMax480Server::instance()->unregisterProvider(this);
}

void VMaxStreamFetcher::onGotArchiveRange(quint32 startDateTime, quint32 endDateTime)
{
    m_res.dynamicCast<QnPlVmax480Resource>()->setArchiveRange(startDateTime * 1000000ll, endDateTime * 1000000ll);
}
