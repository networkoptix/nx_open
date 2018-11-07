#ifdef ENABLE_VMAX

#include "vmax480_stream_fetcher.h"

#include <QtCore/QCoreApplication>

#include "vmax480_tcp_server.h"
#include "utils/common/sleep.h"
#include "core/resource/network_resource.h"
#include "vmax480_resource.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"

static const int PROCESS_TIMEOUT = 1000 * 5;
static const int MAX_QUEUE_SIZE = 150;
static const qint64 EMPTY_PACKET_REPEAT_INTERVAL = 1000ll * 400;
static const qint64 RECONNECT_TIMEOUT_USEC = 4000 * 1000;

QnMutex VMaxStreamFetcher::m_instMutex;
QMap<QByteArray, VMaxStreamFetcher*> VMaxStreamFetcher::m_instances;

void VMaxStreamFetcher::initPacketTime()
{
    memset(m_lastChannelTime, 0, sizeof(m_lastChannelTime));
    m_emptyPacketTime = 0;
}

VMaxStreamFetcher::VMaxStreamFetcher(QnResource* dev, bool isLive):
    m_vMaxProxy(0),
    m_vmaxConnection(0),
    m_isLive(isLive),
    m_usageCount(0),
    m_sequence(0),
    m_lastMediaTime(AV_NOPTS_VALUE),
    m_streamPaused(false),
    m_lastSpeed(1),
    m_lastSeekPos(AV_NOPTS_VALUE),
    m_keepAllChannels(false),
    m_lastConnectTimeUsec(0),
    m_needStop(false)
{
    m_vmaxResource = dev->toSharedPointer().dynamicCast<QnPlVmax480Resource>();
    initPacketTime();
}

VMaxStreamFetcher::~VMaxStreamFetcher()
{
    vmaxDisconnect();
}

bool VMaxStreamFetcher::isOpened() const
{
    return m_vMaxProxy && !m_tcpID.isEmpty() && m_vmaxConnection && m_vmaxConnection->isRunning();
}

void VMaxStreamFetcher::updatePlaybackMask()
{
    std::vector<QnTimePeriodList> allChunks;
    for (ConsumersMap::const_iterator itr = m_dataConsumers.begin(); itr != m_dataConsumers.end(); ++itr)
        allChunks.push_back(itr.key()->chunks());
    m_playbackMaskHelper.setPlaybackMask(QnTimePeriodList::mergeTimePeriods(allChunks));
}

qint64 VMaxStreamFetcher::findRoundTime(qint64 timeUsec, bool* dataFound) const
{
    *dataFound = false;
    qint64 time = m_lastSpeed >= 0 ? DATETIME_NOW : 0;
    for (ConsumersMap::const_iterator itr = m_dataConsumers.begin(); itr != m_dataConsumers.end(); ++itr)
    {
        QnTimePeriodList chunks = itr.key()->chunks();
        if (!chunks.empty()) {
            *dataFound = true;
            if (m_lastSpeed >= 0)
                time = qMin(time, chunks.roundTimeToPeriodUSec(timeUsec, true));
            else
                time = qMax(time, chunks.roundTimeToPeriodUSec(timeUsec, false));
        }
    }
    return time;
}

bool VMaxStreamFetcher::vmaxArchivePlay(QnVmax480DataConsumer* /*consumer*/, qint64 timeUsec, int speed)
{
    m_lastSpeed = speed;

    if (!safeOpen())
        return false;

    QnMutexLocker lock( &m_mutex );

    if (timeUsec == m_lastSeekPos && m_seekTimer.elapsed() < 5000)
        return true;
    m_lastSeekPos = timeUsec;
    m_seekTimer.restart();
    m_eofReached = false;

    for(QnDataPacketQueue* dataQueue: m_dataConsumers) {
        if (dataQueue)
            dataQueue->clear();
    }

    bool dataFound = false;
    qint64 time = findRoundTime(timeUsec, &dataFound);
    if (dataFound)
        timeUsec = time;

    ++m_sequence;
    m_vmaxConnection->vMaxArchivePlay(timeUsec, m_sequence, speed);
    initPacketTime();

    checkEOF(timeUsec);

    return true;
}

bool VMaxStreamFetcher::vmaxPlayRange(QnVmax480DataConsumer* /*consumer*/, const QList<qint64>& pointsUsec)
{
    if (!safeOpen())
        return false;

    QnMutexLocker lock( &m_mutex );
    m_vmaxConnection->vmaxPlayRange(pointsUsec, ++m_sequence);
    return true;
}

void VMaxStreamFetcher::onConnectionEstablished(QnVMax480ConnectionProcessor* connection)
{
    QnMutexLocker lock( &m_connectMtx );
    m_vmaxConnection = connection;
    m_vmaxConnectionCond.wakeOne();
}

int VMaxStreamFetcher::getCurrentChannelMask() const
{
    int mask = 0;
    for(QnVmax480DataConsumer* consumer:  m_dataConsumers.keys())
    {
        if (consumer->getChannel() != -1)
            mask |= 1 << consumer->getChannel();
    }
    return mask;
}

bool VMaxStreamFetcher::vmaxConnect()
{
    auto resource = m_vmaxResource.toStrongRef();
    if (!resource)
        return false;

    QnVMax480Server* vmax480Server =
        resource->serverModule()->findInstance<QnVMax480Server>();
    if (!vmax480Server)
        return false;

    vmaxDisconnect();

    QStringList args;
    int port = vmax480Server->getPort();
    if (port == -1)
        return false;
    args << QString::number(port);
    m_tcpID = vmax480Server->registerProvider(this);
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
        QElapsedTimer t;
        t.restart();
        QnMutexLocker lock( &m_connectMtx );
        while (!m_vmaxConnection && t.elapsed() < PROCESS_TIMEOUT && !m_needStop)
            m_vmaxConnectionCond.wait(&m_connectMtx, PROCESS_TIMEOUT);

        if (m_vmaxConnection) {
            m_vmaxConnection->vMaxConnect(
                resource->getUrl(),
                getCurrentChannelMask(),
                resource->getAuth(),
                m_isLive);
            return true;
        }
    }

    delete m_vMaxProxy;
    m_vMaxProxy = 0;
    vmax480Server->unregisterProvider(this);
    return false;
}

void VMaxStreamFetcher::vmaxDisconnect()
{
    initPacketTime();
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

    if (const auto resource = m_vmaxResource.toStrongRef())
    {
        if (auto vmax480Server = resource->serverModule()->findInstance<QnVMax480Server>())
            vmax480Server->unregisterProvider(this);
    }

    m_lastSeekPos = AV_NOPTS_VALUE;
    m_eofReached = false;
}

void VMaxStreamFetcher::onGotArchiveRange(quint32 startDateTime, quint32 endDateTime)
{
    auto resource = m_vmaxResource.toStrongRef();
    if (!resource)
        return;

    resource->setArchiveRange(startDateTime * 1000000ll, endDateTime * 1000000ll);

    QnMutexLocker lock( &m_mutex );
    for(QnVmax480DataConsumer* consumer: m_dataConsumers.keys())
        consumer->onGotArchiveRange(startDateTime, endDateTime);
}

void VMaxStreamFetcher::onGotMonthInfo(const QDate& month, int monthInfo)
{
    QnMutexLocker lock( &m_mutex );
    for(QnVmax480DataConsumer* consumer: m_dataConsumers.keys())
        consumer->onGotMonthInfo(month, monthInfo);
}

void VMaxStreamFetcher::onGotDayInfo(int dayNum, const QByteArray& data)
{
    QnMutexLocker lock( &m_mutex );
    for(QnVmax480DataConsumer* consumer: m_dataConsumers.keys())
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

bool VMaxStreamFetcher::isEOF() const
{
    return m_eofReached;
}

QnAbstractMediaDataPtr VMaxStreamFetcher::createEmptyPacket(qint64 timestamp)
{
    QnAbstractMediaDataPtr rez(new QnEmptyMediaData());
    rez->timestamp = timestamp; //m_lastSpeed >= 0 ? DATETIME_NOW : 0;
    rez->flags |= QnAbstractMediaData::MediaFlags_PlayUnsync;
    return rez;
}

int VMaxStreamFetcher::getMaxQueueSize() const
{
    int maxQueueSize = 0;
    QnMutexLocker lock( &m_mutex );
    for (ConsumersMap::const_iterator itr = m_dataConsumers.constBegin(); itr != m_dataConsumers.constEnd(); ++itr)
    {
        //QnVmax480DataConsumer* consumer = itr.key();
        QnDataPacketQueue* queue = itr.value();
        maxQueueSize = qMax(queue->size(), maxQueueSize);
    }
    return maxQueueSize;
}

int sign(int value)
{
    return value >= 0 ? 1 : -1;
}

void VMaxStreamFetcher::checkEOF(qint64 timestamp)
{
    qint64 endTimeMs = m_playbackMaskHelper.endTimeMs();
    if (!m_isLive && endTimeMs != (qint64)AV_NOPTS_VALUE && timestamp > (endTimeMs - 60 * 1000) * 1000ll && m_lastSpeed >= 0)
        m_eofReached = true;
}

void VMaxStreamFetcher::onGotData(QnAbstractMediaDataPtr mediaData)
{
    if (mediaData->opaque != m_sequence)
        return;

    m_lastMediaTime = mediaData->timestamp;
    if (m_emptyPacketTime == 0)
        m_emptyPacketTime = m_lastMediaTime;
    else if (m_lastSpeed >= 0)
        m_emptyPacketTime = qMax(m_emptyPacketTime, m_lastMediaTime);
    else
        m_emptyPacketTime = qMin(m_emptyPacketTime, m_lastMediaTime);

    int ch = mediaData->channelNumber & 0xff;

    QElapsedTimer t;
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
        QnMutexLocker lock( &m_mutex );

        if (mediaData->opaque != m_sequence)
            return;
        mediaData->opaque = 0;

        checkEOF(mediaData->timestamp);

#if 0
        qint64 newTime = m_playbackMaskHelper.findTimeAtPlaybackMask(mediaData->timestamp, m_lastSpeed >= 0);
        if (newTime != mediaData->timestamp && newTime != DATETIME_NOW && newTime != -1)
        {
            qDebug() << "outofplayback. srcTime=" << QDateTime::fromMSecsSinceEpoch(mediaData->timestamp/1000).toString(lit("dd/MM/yyyy hh:mm:ss"));
            qDebug() << "outofplayback. roundTime=" << QDateTime::fromMSecsSinceEpoch(newTime/1000).toString(lit("dd/MM/yyyy hh:mm:ss"));
            m_vmaxConnection->vMaxArchivePlay(newTime, ++m_sequence, m_lastSpeed);
            return;
        }
#endif

        qint64 ct = qnSyncTime->currentUSecsSinceEpoch();
        m_lastChannelTime[ch] = ct;

        mediaData->channelNumber = 0;
        mediaData->flags |= QnAbstractMediaData::MediaFlags_PlayUnsync;
        bool isDataUsed = false;
        for (ConsumersMap::Iterator itr = m_dataConsumers.begin(); itr != m_dataConsumers.end(); ++itr)
        {
            QnVmax480DataConsumer* consumer = itr.key();
            int curChannel = consumer->getChannel();
            if (curChannel == ch)
            {
                if (isDataUsed)
                    itr.value()->push(QnAbstractMediaDataPtr(mediaData->clone()));
                else
                    itr.value()->push(mediaData);
                isDataUsed = true;
            }
            else if (ct - m_lastChannelTime[curChannel] > EMPTY_PACKET_REPEAT_INTERVAL && itr.value()->size() < 5) {
                if (m_lastChannelTime[curChannel])
                    itr.value()->push(createEmptyPacket(m_emptyPacketTime));
                m_lastChannelTime[curChannel] = ct;
            }
        }
    }
}

bool VMaxStreamFetcher::registerConsumer(QnVmax480DataConsumer* consumer, int* count, bool openAllChannels, bool checkPlaybackMask)
{
    if (!safeOpen())
        return false;

    QnMutexLocker lock( &m_mutex );
    m_dataConsumers.insert(consumer, new QnDataPacketQueue(MAX_QUEUE_SIZE));
    if (count) {
        *count = 0;
        for(QnVmax480DataConsumer* c: m_dataConsumers.keys())
        {
            if (!c->isStopping())
                (*count)++;
        }
    }

    int channel = consumer->getChannel();
    if (openAllChannels) {
        m_vmaxConnection->vMaxAddChannel(OPEN_ALL);
        m_keepAllChannels = true;
    }
    else if (channel != -1)
    {
        m_vmaxConnection->vMaxAddChannel(1 << channel);
        m_lastChannelTime[channel] = qnSyncTime->currentUSecsSinceEpoch();
    }
    if (checkPlaybackMask)
        updatePlaybackMask();
    return true;
}

int VMaxStreamFetcher::getChannelUsage(int ch)
{
    int channelUsage = 0;
    for(QnVmax480DataConsumer* c: m_dataConsumers.keys())
    {
        if (c->getChannel() == ch)
            channelUsage++;
    }
    return channelUsage;
}

void VMaxStreamFetcher::unregisterConsumer(QnVmax480DataConsumer* consumer)
{
    int ch = consumer->getChannel();

    QnMutexLocker lock( &m_mutex );

    ConsumersMap::Iterator itr = m_dataConsumers.find(consumer);
    if (itr != m_dataConsumers.end()) {
        delete itr.value();
        m_dataConsumers.erase(itr);
    }

    if (m_vmaxConnection)
    {
        if (ch != -1 && !m_keepAllChannels)
        {
            if (getChannelUsage(ch) == 0)
            {
                m_vmaxConnection->vMaxRemoveChannel(1 << ch);
            }
        }
    }

    updatePlaybackMask();

    if (!m_dataConsumers.isEmpty() && !m_isLive && m_lastMediaTime != (qint64)AV_NOPTS_VALUE)
    {
        bool dataFound = false;
        qint64 time = findRoundTime(m_lastMediaTime, &dataFound);
        if (dataFound && time != m_lastMediaTime)
            m_vmaxConnection->vMaxArchivePlay(time, m_sequence, m_lastSpeed);
    }

    //if (m_dataConsumers.isEmpty())
    //    m_lastMediaTime = AV_NOPTS_VALUE;
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

VMaxStreamFetcher* VMaxStreamFetcher::getInstance(const QByteArray& clientGroupID, QnResource* res, bool isLive)
{
    QByteArray vmaxIP = QUrl(res->getUrl()).host().toUtf8();
    const QByteArray key = getInstanceKey(clientGroupID, vmaxIP, isLive);

    QnMutexLocker lock( &m_instMutex );
    VMaxStreamFetcher* result = m_instances.value(key);
    if (result == 0) {
        result = new VMaxStreamFetcher(res, isLive);
        m_instances.insert(key, result);
    }
    result->inUse();
    return result;
}

void VMaxStreamFetcher::freeInstance(const QByteArray& clientGroupID, QnResource* res, bool isLive)
{
    QByteArray vmaxIP = QUrl(res->getUrl()).host().toUtf8();
    const QByteArray key = getInstanceKey(clientGroupID, vmaxIP, isLive);

    VMaxStreamFetcher* result = 0;
    {
        QnMutexLocker lock( &m_instMutex );
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
    QnMutexLocker lock( &m_mutex );
    if (m_needStop)
        return false;
    if (!isOpened())
    {
        qint64 timeoutUsec = getUsecTimer() - m_lastConnectTimeUsec;
        if (timeoutUsec < RECONNECT_TIMEOUT_USEC)
            QnSleep::msleep((RECONNECT_TIMEOUT_USEC - timeoutUsec)/1000); // prevent reconnect flood
        m_lastConnectTimeUsec = getUsecTimer();
        if (!vmaxConnect()) {
            return false;
        }
        if (!m_isLive && m_lastMediaTime != (qint64)AV_NOPTS_VALUE)
            m_vmaxConnection->vMaxArchivePlay(m_lastMediaTime, m_sequence, m_lastSpeed);
    }

    return true;
}

QnAbstractDataPacketPtr VMaxStreamFetcher::getNextData(QnVmax480DataConsumer* consumer)
{
    if (!safeOpen())
        return QnAbstractDataPacketPtr();

    QnDataPacketQueue* dataQueue = 0;
    {
        QnMutexLocker lock( &m_mutex );
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

bool VMaxStreamFetcher::isPlaying() const
{
    QnMutexLocker lock( &m_mutex );
    return m_lastSeekPos != (qint64)AV_NOPTS_VALUE;
}

void VMaxStreamFetcher::pleaseStop()
{
    QnMutexLocker lock( &m_connectMtx );
    m_needStop = true;
    m_vmaxConnectionCond.wakeAll();
}

void VMaxStreamFetcher::pleaseStopAll()
{
    QnMutexLocker lock( &m_instMutex );
    for(VMaxStreamFetcher* fetcher: m_instances.values())
        fetcher->pleaseStop();
}

#endif // #ifdef ENABLE_VMAX
