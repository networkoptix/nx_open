#include "vmax480_resource.h"
#include "vmax480_live_reader.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "vmax480_archive_delegate.h"
#include "vmax480_chunk_reader.h"
#include "core/resource_managment/resource_pool.h"

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
    delete m_chunkReader;
}

int QnPlVmax480Resource::getMaxFps() 
{
    return 30;
}

bool QnPlVmax480Resource::isResourceAccessible() 
{
    return true;
}

bool QnPlVmax480Resource::updateMACAddress() 
{
    return true;
}

QString QnPlVmax480Resource::manufacture() const 
{
    return QLatin1String(MANUFACTURE);
}

void QnPlVmax480Resource::setIframeDistance(int frames, int timems) 
{

}

QString QnPlVmax480Resource::getHostAddress() const 
{
    return QString();
}

bool QnPlVmax480Resource::setHostAddress(const QString &ip, QnDomain domain) 
{
    QUrl url(getUrl());
    url.setHost(ip);
    setUrl(url.toString());
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
    QnArchiveStreamReader* reader = new QnArchiveStreamReader(toSharedPointer());
    reader->setArchiveDelegate(new QnVMax480ArchiveDelegate(toSharedPointer()));
    return reader; 
}

void QnPlVmax480Resource::setCropingPhysical(QRect croping)
{

}

bool QnPlVmax480Resource::initInternal()
{

    Qn::CameraCapabilities addFlags = Qn::PrimaryStreamSoftMotionCapability;
    setCameraCapabilities(getCameraCapabilities() | addFlags);
    save();

    return true;
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

void QnPlVmax480Resource::setArchiveRange(qint64 startTimeUsec, qint64 endTimeUsec)
{
    QMutexLocker lock(&m_mutex);
    m_startTime = startTimeUsec;
    m_endTime = endTimeUsec;
}

void QnPlVmax480Resource::setStatus(Status newStatus, bool silenceMode)
{
    Status oldStatus = getStatus();
    bool isOldOnline = oldStatus == QnResource::Online || oldStatus == QnResource::Recording;

    QnPhysicalCameraResource::setStatus(newStatus, silenceMode);
    if (getChannel() == 0)
    {
        if (!m_chunkReader) {
            m_chunkReader = new QnVMax480ChunkReader(toSharedPointer());
            connect(m_chunkReader, SIGNAL(gotChunks(int, QnTimePeriodList)), this, SLOT(at_gotChunks(int, QnTimePeriodList)));
        }
        bool isNewOnline = getStatus() == QnResource::Online || getStatus() == QnResource::Recording;
        if (isNewOnline && !isOldOnline)
            m_chunkReader->start();
        else
            m_chunkReader->stop();
    }
}

void QnPlVmax480Resource::at_gotChunks(int channel, QnTimePeriodList chunks)
{
    if (channel == getChannel())
        setChunks(chunks);
    else {
        QString suffix = QString(QLatin1String("?channel=%1")).arg(channel+1);
        QString url = getUrl();
        url = url.left(url.indexOf(L'?')+1) + suffix;
        QnPlVmax480ResourcePtr otherRes = qnResPool->getResourceByUrl(url).dynamicCast<QnPlVmax480Resource>();
        if (otherRes)
            otherRes->setChunks(chunks);
    }
}

void QnPlVmax480Resource::setChunks(const QnTimePeriodList& chunks)
{
    QMutexLocker lock(&m_mutexChunks);
    m_chunks = chunks;
    m_chunksReady = true;
    m_chunksCond.wakeAll();
}

QnTimePeriodList QnPlVmax480Resource::getDtsTimePeriods(qint64 startTimeMs, qint64 endTimeMs, int detailLevel) 
{
    QnTimePeriod period(startTimeMs, endTimeMs - startTimeMs);
    
    QMutexLocker lock(&m_mutexChunks);
    while (!m_chunksReady)
        m_chunksCond.wait(&m_mutexChunks);

    return m_chunks.intersected(period);
}
