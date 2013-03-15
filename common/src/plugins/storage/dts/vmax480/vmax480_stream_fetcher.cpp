#include "vmax480_stream_fetcher.h"
#include "vmax480_tcp_server.h"
#include "utils/common/sleep.h"
#include "core/resource/network_resource.h"
#include "vmax480_resource.h"
#include "utils/common/synctime.h"

static const int PROCESS_TIMEOUT = 1000;
static const int MAX_QUEUE_SIZE = 150;

QMutex VMaxStreamFetcher::m_instMutex;
QMap<QByteArray, VMaxStreamFetcher*> VMaxStreamFetcher::m_instances;


VMaxStreamFetcher::VMaxStreamFetcher(QnResourcePtr dev, bool isLive):
    m_vMaxProxy(0),
    m_vmaxConnection(0),
    m_isLive(isLive),
    m_usageCount(0),
    m_sequence(0),
    m_lastMediaTime(AV_NOPTS_VALUE),
    m_streamPaused(false),
    m_lastSpeed(1),
    m_lastSeekPos(AV_NOPTS_VALUE)
{
    m_res = dev.dynamicCast<QnNetworkResource>();
    memset(m_lastChannelTime, 0, sizeof(m_lastChannelTime));
}

VMaxStreamFetcher::~VMaxStreamFetcher()
{
    vmaxDisconnect();
}

bool VMaxStreamFetcher::isOpened() const
{
    return m_vMaxProxy && !m_tcpID.isEmpty() && m_vmaxConnection && m_vmaxConnection->isRunning();
}

bool VMaxStreamFetcher::vmaxArchivePlay(QnVmax480DataConsumer* consumer, qint64 timeUsec, int speed)
{
    m_lastSpeed = speed;

    if (!safeOpen())
        return false;


    int ch = consumer->getChannel();

    QMutexLocker  lock(&m_mutex);

    if (timeUsec == m_lastSeekPos && m_seekTimer.elapsed() < 5000)
        return true;
    m_lastSeekPos = timeUsec;
    m_seekTimer.restart();


    CLDataQueue* dataQueue = m_dataConsumers.value(consumer);
    if (dataQueue)
        dataQueue->clear();

    qint64 time = m_lastSpeed >= 0 ? DATETIME_NOW : 0;
    bool dataFound = false;
    for (ConsumersMap::const_iterator itr = m_dataConsumers.begin(); itr != m_dataConsumers.end(); ++itr)
    {
        QnTimePeriodList chunks = itr.key()->chunks();
        if (!chunks.isEmpty()) {
            dataFound = true;
            if (m_lastSpeed >= 0)
                time = qMin(time, chunks.roundTimeToPeriodUSec(timeUsec, true));
            else
                time = qMax(time, chunks.roundTimeToPeriodUSec(timeUsec, false));
        }
    }
    if (dataFound)
        timeUsec = time;

    ++m_sequence;
    m_vmaxConnection->vMaxArchivePlay(timeUsec, m_sequence, speed);

    return true;
}

bool VMaxStreamFetcher::vmaxPlayRange(QnVmax480DataConsumer* consumer, const QList<qint64>& pointsUsec)
{
    if (!safeOpen())
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

int VMaxStreamFetcher::getCurrentChannelMask() const
{
    int mask = 0;
    foreach(QnVmax480DataConsumer* consumer,  m_dataConsumers.keys())
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
    memset(m_lastChannelTime, 0, sizeof(m_lastChannelTime));
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
    m_lastMediaTime = AV_NOPTS_VALUE;
    m_lastSeekPos = AV_NOPTS_VALUE;
}

void VMaxStreamFetcher::onGotArchiveRange(quint32 startDateTime, quint32 endDateTime)
{
    m_res.dynamicCast<QnPlVmax480Resource>()->setArchiveRange(startDateTime * 1000000ll, endDateTime * 1000000ll);

    QMutexLocker lock(&m_mutex);
    foreach(QnVmax480DataConsumer* consumer, m_dataConsumers.keys())
        consumer->onGotArchiveRange(startDateTime, endDateTime);
}

void VMaxStreamFetcher::onGotMonthInfo(const QDate& month, int monthInfo)
{
    QMutexLocker lock(&m_mutex);
    foreach(QnVmax480DataConsumer* consumer, m_dataConsumers.keys())
        consumer->onGotMonthInfo(month, monthInfo);
}

void VMaxStreamFetcher::onGotDayInfo(int dayNum, const QByteArray& data)
{
    QMutexLocker lock(&m_mutex);
    foreach(QnVmax480DataConsumer* consumer, m_dataConsumers.keys())
        consumer->onGotDayInfo(dayNum, data);
}

bool VMaxStreamFetcher::vmaxRequestMonthInfo(const QDate& month)
{
    if (!safeOpen())
        return false;
    if (m_vmaxConnection)
        m_vmaxConnection->vMaxRequestMonthInfo(month);
    return true;
}

bool VMaxStreamFetcher::vmaxRequestDayInfo(int dayNum)
{
    if (!safeOpen())
        return false;
    if (m_vmaxConnection)
        m_vmaxConnection->vMaxRequestDayInfo(dayNum);
    return true;
}

bool VMaxStreamFetcher::vmaxRequestRange()
{
    if (!safeOpen())
        return false;

    if (m_vmaxConnection)
        m_vmaxConnection->vMaxRequestRange();

    return true;
}

QnAbstractMediaDataPtr VMaxStreamFetcher::createEmptyPacket()
{
    QnAbstractMediaDataPtr rez(new QnEmptyMediaData());
    rez->timestamp = m_lastSpeed >= 0 ? DATETIME_NOW : 0;
    rez->flags |= QnAbstractMediaData::MediaFlags_PlayUnsync;
    return rez;
}

int VMaxStreamFetcher::getMaxQueueSize() const
{
    int maxQueueSize = 0;
    QMutexLocker lock(&m_mutex);
    for (ConsumersMap::const_iterator itr = m_dataConsumers.constBegin(); itr != m_dataConsumers.constEnd(); ++itr)
    {
        QnVmax480DataConsumer* consumer = itr.key();
        CLDataQueue* queue = itr.value();
        maxQueueSize = qMax(queue->size(), maxQueueSize);
    }
    return maxQueueSize;
}

void VMaxStreamFetcher::onGotData(QnAbstractMediaDataPtr mediaData)
{
    if (mediaData->opaque != m_sequence)
        return;

    mediaData->opaque = 0;
    m_lastMediaTime = mediaData->timestamp;

    int ch = mediaData->channelNumber & 0xff;

    QTime t;
    t.restart();


    bool needWait = false;
    int maxQueueSize = 0;
    do 
    {
        maxQueueSize = getMaxQueueSize();
        needWait = maxQueueSize > 10 && t.elapsed() < 1000 * 10;
        if (needWait)
            QnSleep::msleep(1);
    } while (needWait);

    if (maxQueueSize > MAX_QUEUE_SIZE && !m_streamPaused) {
        m_vmaxConnection->vMaxArchivePlay(m_lastMediaTime, m_sequence, 0);
        m_streamPaused = true;
    }

    {
        QMutexLocker lock(&m_mutex);

        qint64 ct = qnSyncTime->currentUSecsSinceEpoch();
        m_lastChannelTime[ch] = ct;

        mediaData->channelNumber = 0;
        mediaData->flags |= QnAbstractMediaData::MediaFlags_PlayUnsync;
        for (ConsumersMap::Iterator itr = m_dataConsumers.begin(); itr != m_dataConsumers.end(); ++itr)
        {
            QnVmax480DataConsumer* consumer = itr.key();
            int curChannel = consumer->getChannel();
            if (curChannel == ch) 
            {
                itr.value()->push(mediaData);
            }
            else if (ct - m_lastChannelTime[curChannel] > 1000000ll) {
                m_lastChannelTime[curChannel] = ct;
                itr.value()->push(createEmptyPacket());
            }
        }
    }
}

bool VMaxStreamFetcher::registerConsumer(QnVmax480DataConsumer* consumer)
{
    if (!safeOpen())
        return false;

    QMutexLocker lock(&m_mutex);
    m_dataConsumers.insert(consumer, new CLDataQueue(MAX_QUEUE_SIZE));

    int channel = consumer->getChannel();
    if (channel != -1) 
    {
        if (getChannelUsage(channel) == 1) {
            m_vmaxConnection->vMaxAddChannel(1 << channel);
            m_lastChannelTime[channel] = qnSyncTime->currentUSecsSinceEpoch();

        }
    }
    return true;
}

int VMaxStreamFetcher::getChannelUsage(int ch)
{
    int channelUsage = 0;
    foreach(QnVmax480DataConsumer* c, m_dataConsumers.keys())
    {
        if (c->getChannel() == ch)
            channelUsage++;
    }
    return channelUsage;
}

void VMaxStreamFetcher::unregisterConsumer(QnVmax480DataConsumer* consumer)
{
    int ch = consumer->getChannel();

    QMutexLocker lock(&m_mutex);

    ConsumersMap::Iterator itr = m_dataConsumers.find(consumer);
    if (itr != m_dataConsumers.end()) {
        delete itr.value();
        m_dataConsumers.erase(itr);
    }

    if (m_vmaxConnection)
    {
        if (ch != -1) {
            if (getChannelUsage(ch) == 1) 
            {
                m_vmaxConnection->vMaxRemoveChannel(1 << ch);
            }
        }
    }
}

QByteArray getInstanceKey(const QByteArray& clientGroupID, const QByteArray& vmaxIP, bool isLive)
{
    return clientGroupID + QByteArray("-") + vmaxIP + QByteArray("-") + (isLive ? QByteArray("1") : QByteArray("0"));
};

void VMaxStreamFetcher::inUse()
{
    m_usageCount++;
}

void VMaxStreamFetcher::notInUse()
{
    m_usageCount--;
}

VMaxStreamFetcher* VMaxStreamFetcher::getInstance(const QByteArray& clientGroupID, QnResourcePtr res, bool isLive)
{
    QByteArray vmaxIP = QUrl(res->getUrl()).host().toUtf8();
    const QByteArray key = getInstanceKey(clientGroupID, vmaxIP, isLive);

    QMutexLocker lock(&m_instMutex);
    VMaxStreamFetcher* result = m_instances.value(key);
    if (result == 0) {
        result = new VMaxStreamFetcher(res, isLive);
        m_instances.insert(key, result);
    }
    result->inUse();
    return result;
}

void VMaxStreamFetcher::freeInstance(const QByteArray& clientGroupID, QnResourcePtr res, bool isLive)
{
    QByteArray vmaxIP = QUrl(res->getUrl()).host().toUtf8();
    const QByteArray key = getInstanceKey(clientGroupID, vmaxIP, isLive);

    VMaxStreamFetcher* result = 0;
    {
        QMutexLocker lock(&m_instMutex);
        result = m_instances.value(key);
        if (result) {
            result->notInUse();
            if (result->usageCount() == 0) {
                m_instances.remove(key);
            }
            else {
                result = 0;
            }
        }
    }

    if (result)
        delete result;
}

bool VMaxStreamFetcher::safeOpen()
{
    QMutexLocker lock(&m_mutex);
    if (!isOpened()) {
        if (!vmaxConnect())
            return false;
        if (!m_isLive && m_lastMediaTime != AV_NOPTS_VALUE)
            m_vmaxConnection->vMaxArchivePlay(m_lastMediaTime, m_sequence, m_lastSpeed);
    }

    return true;
}

QnAbstractDataPacketPtr VMaxStreamFetcher::getNextData(QnVmax480DataConsumer* consumer)
{
    if (!safeOpen()) 
        return QnAbstractDataPacketPtr();
            
    CLDataQueue* dataQueue = 0;
    {
        QMutexLocker lock(&m_mutex);
        ConsumersMap::Iterator itr = m_dataConsumers.find(consumer);
        if (itr == m_dataConsumers.end())
            return QnAbstractDataPacketPtr();
        dataQueue = itr.value();
    }
    QnAbstractDataPacketPtr result;
    dataQueue->pop(result, 10);

    if (result && m_streamPaused)
    {
        int maxQueueSize = getMaxQueueSize();
        if (maxQueueSize < MAX_QUEUE_SIZE / 4) {
            m_streamPaused = false;
            m_vmaxConnection->vMaxArchivePlay(m_lastMediaTime, m_sequence, m_lastSpeed);
        }
    }

    return result;
}
