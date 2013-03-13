#include "vmax480_stream_fetcher.h"
#include "vmax480_tcp_server.h"
#include "utils/common/sleep.h"
#include "core/resource/network_resource.h"
#include "vmax480_resource.h"

static const int PROCESS_TIMEOUT = 1000;

QMutex VMaxStreamFetcher::m_instMutex;
QMap<QString, VMaxStreamFetcher*> VMaxStreamFetcher::m_instances;


VMaxStreamFetcher::VMaxStreamFetcher(QnResourcePtr dev, bool isLive):
    m_vMaxProxy(0),
    m_vmaxConnection(0),
    m_isLive(isLive),
    m_usageCount(0),
    m_sequence(0),
    m_mainConsumer(0)
{
    m_res = dev.dynamicCast<QnNetworkResource>();
    start();
}

VMaxStreamFetcher::~VMaxStreamFetcher()
{
    vmaxDisconnect();
    stop();
}

bool VMaxStreamFetcher::isOpened() const
{
    return m_vMaxProxy && !m_tcpID.isEmpty() && m_vmaxConnection && m_vmaxConnection->isRunning();
}

bool VMaxStreamFetcher::vmaxArchivePlay(QnVmax480DataConsumer* consumer, qint64 timeUsec, int speed)
{
    if (!waitForConnected())
        return false;


    int ch = consumer->getChannel();

    QMutexLocker  lock(&m_mutex);

    if (consumer == m_mainConsumer) {
        ++m_sequence;
        foreach(QnVmax480DataConsumer* c, m_dataConsumers)
            c->beforeSeek();
        m_vmaxConnection->vMaxArchivePlay(timeUsec, m_sequence, speed);
    }

    return true;
}

bool VMaxStreamFetcher::vmaxPlayRange(QnVmax480DataConsumer* consumer, const QList<qint64>& pointsUsec)
{
    if (!waitForConnected())
        return false;

    QMutexLocker  lock(&m_mutex);
    m_vmaxConnection->vmaxPlayRange(pointsUsec, ++m_sequence);
    return true;
}

void VMaxStreamFetcher::onConnectionEstablished(QnVMax480ConnectionProcessor* connection)
{
    QMutexLocker lock(&m_connectMtx);
    m_vmaxConnection = connection;
    m_vmaxConnectionCond.wakeOne();
}

bool VMaxStreamFetcher::waitForConnected()
{
    QTime t;
    t.restart();
    while (!isOpened() && t.elapsed() < 5000)
        msleep(1);
    return isOpened();
}

void VMaxStreamFetcher::doExtraDelay()
{
    for (int i = 0; i < 100 && !needToStop(); ++i)
        msleep(10);
}

void VMaxStreamFetcher::run()
{
    while (!needToStop()) 
    {
        {
            QMutexLocker lock(&m_mutex);
            if (m_dataConsumers.isEmpty())
                vmaxDisconnect();
            else if (!isOpened()) {
                vmaxConnect();
                if (!isOpened())
                    doExtraDelay();
            }
        }
        msleep(10);
    }
}

int VMaxStreamFetcher::getCurrentChannelMask() const
{
    int mask = 0;
    foreach(QnVmax480DataConsumer* consumer,  m_dataConsumers)
    {
        if (consumer->getChannel() != -1)
            mask |= 1 << consumer->getChannel();
    }
    return mask;
}

bool VMaxStreamFetcher::vmaxConnect()
{
    vmaxDisconnect();

    QStringList args;
    args << QString::number(QnVMax480Server::instance()->getPort());
    m_tcpID = QnVMax480Server::instance()->registerProvider(this);
    args << m_tcpID;
    m_vMaxProxy = new QProcess();

    const QString execStr = QCoreApplication::applicationDirPath() + QLatin1String("/vmaxproxy/vmaxproxy");
#if 0
    m_vMaxProxy->start(execStr, args);
    if (m_vMaxProxy->waitForStarted(PROCESS_TIMEOUT))
#else
    m_vMaxProxy->startDetached(execStr, args); 
    if (m_vMaxProxy->waitForStarted(PROCESS_TIMEOUT) || true)
#endif
    {
        QTime t;
        t.restart();
        QMutexLocker lock(&m_connectMtx);
        while (!m_vmaxConnection && t.elapsed() < PROCESS_TIMEOUT)
            m_vmaxConnectionCond.wait(&m_connectMtx, PROCESS_TIMEOUT);

        if (m_vmaxConnection) {
            m_vmaxConnection->vMaxConnect(m_res->getUrl(), getCurrentChannelMask(), m_res->getAuth(), m_isLive);
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

    QMutexLocker lock(&m_mutex);
    foreach(QnVmax480DataConsumer* consumer, m_dataConsumers)
        consumer->onGotArchiveRange(startDateTime, endDateTime);
}

void VMaxStreamFetcher::onGotMonthInfo(const QDate& month, int monthInfo)
{
    QMutexLocker lock(&m_mutex);
    foreach(QnVmax480DataConsumer* consumer, m_dataConsumers)
        consumer->onGotMonthInfo(month, monthInfo);
}

void VMaxStreamFetcher::onGotDayInfo(int dayNum, const QByteArray& data)
{
    QMutexLocker lock(&m_mutex);
    foreach(QnVmax480DataConsumer* consumer, m_dataConsumers)
        consumer->onGotDayInfo(dayNum, data);
}

bool VMaxStreamFetcher::vmaxRequestMonthInfo(const QDate& month)
{
    if (!waitForConnected())
        return false;
    if (m_vmaxConnection)
        m_vmaxConnection->vMaxRequestMonthInfo(month);
    return true;
}

bool VMaxStreamFetcher::vmaxRequestDayInfo(int dayNum)
{
    if (!waitForConnected())
        return false;
    if (m_vmaxConnection)
        m_vmaxConnection->vMaxRequestDayInfo(dayNum);
    return true;
}

bool VMaxStreamFetcher::vmaxRequestRange()
{
    if (!waitForConnected())
        return false;

    if (m_vmaxConnection)
        m_vmaxConnection->vMaxRequestRange();

    return true;
}

void VMaxStreamFetcher::onGotData(QnAbstractMediaDataPtr mediaData)
{
    QMutexLocker lock(&m_mutex);

    if (mediaData->opaque != m_sequence)
        return;
    mediaData->opaque = 0;

    int ch = mediaData->channelNumber;
    mediaData->channelNumber = 0;
    mediaData->flags |= QnAbstractMediaData::MediaFlags_PlayUnsync;
    foreach(QnVmax480DataConsumer* consumer, m_dataConsumers)
    {
        if (consumer->getChannel() == ch)
            consumer->onGotData(mediaData);
    }
}

bool VMaxStreamFetcher::registerConsumer(QnVmax480DataConsumer* consumer)
{
    /*
    QMutexLocker lock(&m_mutex);


    if (m_vmaxConnection == 0) {
        vmaxConnect();
        if (m_vmaxConnection == 0)
            return;
    }

    if (consumer->getChannel() != -1)
        m_vmaxConnection->vMaxAddChannel(consumer->getChannel());
    m_dataConsumers << consumer;
    */

    {
        QMutexLocker lock(&m_mutex);
        m_dataConsumers << consumer;
        if (m_mainConsumer == 0)
            m_mainConsumer = consumer;
    }
    if (waitForConnected()) {
        if (consumer->getChannel() != -1) 
        {
            QMutexLocker lock(&m_mutex);
            if (getChannelUsage(consumer->getChannel()) == 1)
                m_vmaxConnection->vMaxAddChannel(1 << consumer->getChannel());
        }
        return true;
    }
    else {
        unregisterConsumer(consumer);
        return false;
    }
}

int VMaxStreamFetcher::getChannelUsage(int ch)
{
    int channelUsage = 0;
    foreach(QnVmax480DataConsumer* c, m_dataConsumers)
    {
        if (c->getChannel() == ch)
            channelUsage++;
    }
    return channelUsage;
}

void VMaxStreamFetcher::unregisterConsumer(QnVmax480DataConsumer* consumer)
{
    QMutexLocker lock(&m_mutex);

    if (m_vmaxConnection)
    {
        int ch = consumer->getChannel();
        if (ch != -1) {
            if (getChannelUsage(ch) == 1)
                m_vmaxConnection->vMaxRemoveChannel(1 << consumer->getChannel());
        }
    }
    m_dataConsumers.remove(consumer);
    if (m_mainConsumer == consumer) {
        m_mainConsumer = 0;
        if (!m_dataConsumers.isEmpty())
            m_mainConsumer = *m_dataConsumers.begin();
    }
    //if (m_dataConsumers.isEmpty())
    //    vmaxDisconnect();
}

QString getInstanceKey(const QString& clientGroupID, const QString& vmaxIP, bool isLive)
{
    return QString(QLatin1String("%1-%2-%3")).arg(clientGroupID).arg(vmaxIP).arg(isLive);
};

void VMaxStreamFetcher::inUse()
{
    m_usageCount++;
}

void VMaxStreamFetcher::notInUse()
{
    m_usageCount--;
}

VMaxStreamFetcher* VMaxStreamFetcher::getInstance(const QString& clientGroupID, QnResourcePtr res, bool isLive)
{
    QString vmaxIP = QUrl(res->getUrl()).host();
    const QString key = getInstanceKey(clientGroupID, vmaxIP, isLive);

    QMutexLocker lock(&m_instMutex);
    VMaxStreamFetcher* result = m_instances.value(key);
    if (result == 0) {
        result = new VMaxStreamFetcher(res, isLive);
        m_instances.insert(key, result);
    }
    result->inUse();
    return result;
}

void VMaxStreamFetcher::freeInstance(const QString& clientGroupID, QnResourcePtr res, bool isLive)
{
    QString vmaxIP = QUrl(res->getUrl()).host();
    const QString key = getInstanceKey(clientGroupID, vmaxIP, isLive);

    QMutexLocker lock(&m_instMutex);
    VMaxStreamFetcher* result = m_instances.value(key);
    if (result) {
        result->notInUse();
        if (result->usageCount() == 0) {
            delete result;
            m_instances.remove(key);
        }
    }
}
