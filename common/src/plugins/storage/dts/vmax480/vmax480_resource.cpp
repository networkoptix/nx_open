#include "vmax480_resource.h"
#include "vmax480_live_reader.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "vmax480_archive_delegate.h"
#include "vmax480_chunk_reader.h"
#include "core/resource_managment/resource_pool.h"

#include <QtCore/QUrlQuery>

QMutex QnPlVmax480Resource::m_chunkReaderMutex;
QMap<QString, QnVMax480ChunkReader*> QnPlVmax480Resource::m_chunkReaderMap;


const char* QnPlVmax480Resource::MANUFACTURE = "VMAX";

QnPlVmax480Resource::QnPlVmax480Resource():
    m_startTime(AV_NOPTS_VALUE),
    m_endTime(AV_NOPTS_VALUE),
    m_chunkReader(0),
    m_chunksReady(false)
{
}

QnPlVmax480Resource::~QnPlVmax480Resource()
{
    QMutexLocker lock(&m_chunkReaderMutex);
    if (m_chunkReader) {
        m_chunkReaderMap.remove(getHostAddress());
        delete m_chunkReader;
    }
}

int QnPlVmax480Resource::getMaxFps() const
{
    return 30;
}

bool QnPlVmax480Resource::isResourceAccessible() 
{
    return true;
}

QString QnPlVmax480Resource::getDriverName() const 
{
    return QLatin1String(MANUFACTURE);
}

void QnPlVmax480Resource::setIframeDistance(int frames, int timems) 
{
    Q_UNUSED(frames)
    Q_UNUSED(timems)
}

bool QnPlVmax480Resource::setHostAddress(const QString &ip, QnDomain domain) 
{
    QString oldHostAddr = getHostAddress();

    Q_UNUSED(domain)
    QUrl url(getUrl());

    if (url.host() == ip)
        return true;

    url.setHost(ip);
    setUrl(url.toString());

    {
        QMutexLocker lock(&m_chunkReaderMutex);
        if (m_chunkReader) {
            m_chunkReaderMap.remove(oldHostAddr);
            m_chunkReaderMap.insert(getHostAddress(), m_chunkReader);
        }
    }

    return true;
}

int QnPlVmax480Resource::channelNum() const
{
    //vmax://ip:videoport:event:channel
    QString url = getUrl();

    QStringList lst = url.split(QLatin1Char(':'));
    if (lst.size() < 5)
        return 0;

    return lst[4].toInt();
}

int QnPlVmax480Resource::videoPort() const
{
    //vmax://ip:videoport:event:channel
    QString url = getUrl();

    QStringList lst = url.split(QLatin1Char(':'));
    if (lst.size() < 5)
        return 0;

    return lst[2].toInt();

}

int QnPlVmax480Resource::eventPort() const
{
    //vmax://ip:videoport:event:channel
    QString url = getUrl();

    QStringList lst = url.split(QLatin1Char(':'));
    if (lst.size() < 5)
        return 0;

    return lst[3].toInt();
    
}

bool QnPlVmax480Resource::shoudResolveConflicts() const 
{
    return false;
}

QnAbstractStreamDataProvider* QnPlVmax480Resource::createLiveDataProvider()
{
    return new QnVMax480LiveProvider(toSharedPointer());
}

QnAbstractStreamDataProvider* QnPlVmax480Resource::createArchiveDataProvider()
{
    QnAbstractArchiveDelegate* archiveDelegate = createArchiveDelegate();
    QnArchiveStreamReader* archiveReader = new QnArchiveStreamReader(toSharedPointer());
    archiveReader->setArchiveDelegate(archiveDelegate);
    if (hasFlags(still_image) || hasFlags(utc))
        archiveReader->setCycleMode(false);
    return archiveReader;
}

QnAbstractArchiveDelegate* QnPlVmax480Resource::createArchiveDelegate() 
{ 
    return new QnVMax480ArchiveDelegate(toSharedPointer());
}

CameraDiagnostics::Result QnPlVmax480Resource::initInternal()
{
    QnPhysicalCameraResource::initInternal();
    Qn::CameraCapabilities addFlags = Qn::PrimaryStreamSoftMotionCapability;
    setCameraCapabilities(getCameraCapabilities() | addFlags);
    save();

    QMutexLocker lock(&m_chunkReaderMutex);
    QnVMax480ChunkReader* chunkReader = m_chunkReaderMap.value(getHostAddress());
    if (chunkReader == 0) {
        m_chunkReader = new QnVMax480ChunkReader(toSharedPointer());
        m_chunkReaderMap.insert(getHostAddress(), m_chunkReader);
        connect(m_chunkReader, SIGNAL(gotChunks(int, QnTimePeriodList)), this, SLOT(at_gotChunks(int, QnTimePeriodList)));
        m_chunkReader->start();
    }

    return CameraDiagnostics::NoErrorResult();
}

qint64 QnPlVmax480Resource::startTime() const
{
    QMutexLocker lock(&m_mutex);
    return m_startTime;
}

qint64 QnPlVmax480Resource::endTime() const
{
    QMutexLocker lock(&m_mutex);
    return m_endTime;
}

void QnPlVmax480Resource::setStartTime(qint64 valueUsec)
{
    QMutexLocker lock(&m_mutex);
    m_startTime = valueUsec;
}

void QnPlVmax480Resource::setEndTime(qint64 valueUsec)
{
    QMutexLocker lock(&m_mutex);
    m_endTime = valueUsec;
}

void QnPlVmax480Resource::setArchiveRange(qint64 startTimeUsec, qint64 endTimeUsec, bool recursive)
{
    {
        QMutexLocker lock(&m_mutex);
        m_startTime = startTimeUsec;
        m_endTime = endTimeUsec;
    }

    if (recursive) 
    {
        for (int i = 0; i < VMAX_MAX_CH; ++i)
        {
            QnPhysicalCameraResourcePtr otherRes = getOtherResource(i);
            if (otherRes && otherRes.data() != this)
                otherRes.dynamicCast<QnPlVmax480Resource>()->setArchiveRange(startTimeUsec, endTimeUsec, false);
        }
    }
}

void QnPlVmax480Resource::setStatus(Status newStatus, bool silenceMode)
{
    QnPhysicalCameraResource::setStatus(newStatus, silenceMode);
}

QnPhysicalCameraResourcePtr QnPlVmax480Resource::getOtherResource(int channel)
{
    QUrl url(getUrl());
    QUrlQuery urlQuery(url.query());
    QList<QPair<QString, QString> > items = urlQuery.queryItems();
    for (int i = 0; i < items.size(); ++i)
    {
        if (items[i].first == lit("channel"))
            items[i].second = QString::number(channel+1);
    }
    urlQuery.setQueryItems(items);
    url.setQuery(urlQuery);
    QString urlStr = url.toString();
    return qnResPool->getResourceByUrl(urlStr).dynamicCast<QnPhysicalCameraResource>();
}

void QnPlVmax480Resource::at_gotChunks(int channel, QnTimePeriodList chunks)
{
    if (channel == getChannel())
        setChunks(chunks);
    else {
        QnPhysicalCameraResourcePtr otherRes = getOtherResource(channel);
        if (otherRes)
            otherRes.dynamicCast<QnPlVmax480Resource>()->setChunks(chunks);
    }
}

QnTimePeriodList  QnPlVmax480Resource::getChunks()
{
    QMutexLocker lock(&m_mutexChunks);
    return m_chunks;
}

void QnPlVmax480Resource::setChunks(const QnTimePeriodList& chunks)
{
    QMutexLocker lock(&m_mutexChunks);
    m_chunks = chunks;
    m_chunksReady = true;
    //m_chunksCond.wakeAll();
}

QnTimePeriodList QnPlVmax480Resource::getDtsTimePeriods(qint64 startTimeMs, qint64 endTimeMs, int detailLevel) 
{
    Q_UNUSED(detailLevel)
    if (!m_chunks.isEmpty())
        startTimeMs = qMin(startTimeMs, m_chunks.last().startTimeMs);

    QnTimePeriod period(startTimeMs, endTimeMs - startTimeMs);
    
    QMutexLocker lock(&m_mutexChunks);
    //while (!m_chunksReady)
    //    m_chunksCond.wait(&m_mutexChunks);

    return m_chunks.intersected(period);
}
