#include "vmax480_stream_fetcher.h"
#include "vmax480_tcp_server.h"
#include "utils/common/sleep.h"
#include "core/resource/network_resource.h"
#include "vmax480_resource.h"

static const int PROCESS_TIMEOUT = 1000;

VMaxStreamFetcher::VMaxStreamFetcher(QnResourcePtr dev):
    m_vMaxProxy(0),
    m_vmaxConnection(0)
{
    m_res = dev.dynamicCast<QnNetworkResource>();
}

VMaxStreamFetcher::~VMaxStreamFetcher()
{
    //vmaxDisconnect();
}

bool VMaxStreamFetcher::isOpened() const
{
    return m_vMaxProxy && !m_tcpID.isEmpty();
}

void VMaxStreamFetcher::vmaxArchivePlay(qint64 timeUsec, quint8 sequence)
{
    m_vmaxConnection->vMaxArchivePlay(timeUsec, sequence);
}

void VMaxStreamFetcher::onConnectionEstablished(QnVMax480ConnectionProcessor* connection)
{
    QMutexLocker lock(&m_connectMtx);
    m_vmaxConnection = connection;
    m_vmaxConnectionCond.wakeOne();
}

bool VMaxStreamFetcher::vmaxConnect(bool isLive, int channel)
{
    QStringList args;
    args << QString::number(QnVMax480Server::instance()->getPort());
    m_tcpID = QnVMax480Server::instance()->registerProvider(this);
    args << m_tcpID;
    m_vMaxProxy = new QProcess();
    
#if 1
    m_vMaxProxy->start(QLatin1String("vmaxproxy"), args);
    if (m_vMaxProxy->waitForStarted(PROCESS_TIMEOUT))
#else
    m_vMaxProxy->startDetached(QLatin1String("vmaxproxy"), args); // debug only!
    if (m_vMaxProxy->waitForStarted(PROCESS_TIMEOUT) || true)     // debug only!
#endif
    {
        QMutexLocker lock(&m_connectMtx);
        while (!m_vmaxConnection)
            m_vmaxConnectionCond.wait(&m_connectMtx, PROCESS_TIMEOUT);

        if (m_vmaxConnection) {
            m_vmaxConnection->vMaxConnect(m_res->getUrl(), channel, m_res->getAuth(), isLive);
            return true;
        }
    }
    

    delete m_vMaxProxy;
    m_vMaxProxy = 0;
    QnVMax480Server::instance()->unregisterProvider(this);
    return false;
}

void VMaxStreamFetcher::vmaxDisconnect()
{
    if (m_vmaxConnection) {
        m_vmaxConnection->vMaxDisconnect();
        delete m_vmaxConnection;
    }
    m_vmaxConnection = 0;

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

void VMaxStreamFetcher::vmaxRequestMonthInfo(const QDate& month)
{
    m_vmaxConnection->vMaxRequestMonthInfo(month);
}

void VMaxStreamFetcher::vmaxRequestDayInfo(int dayNum)
{
    m_vmaxConnection->vMaxRequestDayInfo(dayNum);
}

void VMaxStreamFetcher::vmaxRequestRange()
{
    m_vmaxConnection->vMaxRequestRange();
}
