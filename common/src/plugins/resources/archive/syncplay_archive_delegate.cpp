#include "syncplay_archive_delegate.h"
#include "syncplay_wrapper.h"
#include "abstract_archive_delegate.h"
#include "abstract_archive_stream_reader.h"
#include "utils/common/util.h"

QnSyncPlayArchiveDelegate::QnSyncPlayArchiveDelegate(QnAbstractArchiveReader* reader, QnArchiveSyncPlayWrapper* syncWrapper, QnAbstractArchiveDelegate* ownerDelegate):
    m_reader(reader),
    m_syncWrapper(syncWrapper), 
    m_ownerDelegate(ownerDelegate),
    m_seekTime(AV_NOPTS_VALUE),
    m_enableSync(true),
    m_liveMode(true)
{

}

QnSyncPlayArchiveDelegate::~QnSyncPlayArchiveDelegate()
{
    delete m_ownerDelegate;
    m_syncWrapper->erase(this);
}

bool QnSyncPlayArchiveDelegate::open(QnResourcePtr resource)
{
    return m_ownerDelegate->open(resource);
}

void QnSyncPlayArchiveDelegate::close()
{
    m_ownerDelegate->close();
}

qint64 QnSyncPlayArchiveDelegate::startTime()
{
    return m_syncWrapper->minTime();
}

qint64 QnSyncPlayArchiveDelegate::endTime()
{
    return m_syncWrapper->endTime();
}

qint64 QnSyncPlayArchiveDelegate::jumpToPreviousFrame (qint64 time, bool makeshot)
{
    QMutexLocker lock(&m_genericMutex);
    m_seekTime = time;
    m_reader->jumpToPreviousFrame(time, makeshot);
    return time;
}

qint64 QnSyncPlayArchiveDelegate::seek (qint64 time)
{
    //return m_syncWrapper->seek(time);
    //m_seekTime = AV_NOPTS_VALUE;
    m_tmpData = QnAbstractMediaDataPtr(0);
    m_liveMode = time == DATETIME_NOW;
    return m_ownerDelegate->seek(time);
}

QnVideoResourceLayout* QnSyncPlayArchiveDelegate::getVideoLayout()
{
    return m_ownerDelegate->getVideoLayout();
}

QnResourceAudioLayout* QnSyncPlayArchiveDelegate::getAudioLayout()
{
    return m_ownerDelegate->getAudioLayout();
}

void QnSyncPlayArchiveDelegate::setStartDelay(qint64 startDelay)
{
    m_startDelay = startDelay;
}

qint64 QnSyncPlayArchiveDelegate::secondTime() const
{
    QMutexLocker lock(&m_genericMutex);
    QnCompressedVideoDataPtr vd = qSharedPointerDynamicCast<QnCompressedVideoData>(m_tmpData);
    if (vd && vd->timestamp >= m_reader->skipFramesToTime() && m_seekTime == AV_NOPTS_VALUE)
        return m_tmpData->timestamp;
    else
        return AV_NOPTS_VALUE;
}

QnAbstractMediaDataPtr QnSyncPlayArchiveDelegate::getNextData()
{
    /*
    QnAbstractMediaDataPtr readedData;
    QnAbstractMediaDataPtr mediaData;
    do {
        readedData = m_ownerDelegate->getNextData();
        mediaData = qSharedPointerDynamicCast<QnAbstractMediaData>(readedData);
    } while (m_seekTime != AV_NOPTS_VALUE && mediaData && !(mediaData->flags & QnAbstractMediaData::MediaFlags_BOF));
    */
    QnAbstractMediaDataPtr readedData = m_ownerDelegate->getNextData();

    {
        QMutexLocker lock(&m_genericMutex);
        if (m_seekTime != AV_NOPTS_VALUE) 
        {
            m_tmpData = QnAbstractMediaDataPtr(0);
            QnAbstractMediaDataPtr mediaData = qSharedPointerDynamicCast<QnAbstractMediaData>(readedData);
            if (mediaData && !(mediaData->flags & QnAbstractMediaData::MediaFlags_BOF))
                return readedData;

            //QString msg;
            //QTextStream str(&msg);
            //str << "BOF time=" << QDateTime::fromMSecsSinceEpoch(mediaData->timestamp/1000).toString() << " borderTime=";
            //str.flush();
            //cl_log.log(msg, cl_logWARNING);

            m_seekTime = AV_NOPTS_VALUE;
        }
    }

    QnCompressedVideoDataPtr vd = qSharedPointerDynamicCast<QnCompressedVideoData>(readedData);
    bool isPrimaryVideo = vd && readedData->channelNumber == 0;
    if (readedData)
    {
        if (!isPrimaryVideo)
            return readedData;
        QMutexLocker lock(&m_genericMutex);
        if (m_tmpData == 0) 
        {
            m_tmpData = readedData;
            readedData = m_ownerDelegate->getNextData();
            isPrimaryVideo = qSharedPointerDynamicCast<QnCompressedVideoData>(readedData) && readedData->channelNumber == 0;
            if (readedData && !isPrimaryVideo) {
                m_syncWrapper->onNewDataReaded();
                return readedData;
            }
        }
    }

    //if (m_tmpData && !m_reader->isSingleShotMode())
    if (m_tmpData && m_enableSync && !m_liveMode && vd && vd->timestamp >= m_reader->skipFramesToTime())
        m_syncWrapper->waitIfNeed(m_reader, m_tmpData->timestamp);

    QMutexLocker lock(&m_genericMutex);
    QnAbstractMediaDataPtr rez = m_tmpData;
    m_tmpData = readedData;
    m_syncWrapper->onNewDataReaded();
    return rez;
}

AVCodecContext* QnSyncPlayArchiveDelegate::setAudioChannel(int num)
{
    // play synchronized movies without audio
    return 0;
}

void QnSyncPlayArchiveDelegate::enableSync(bool value)
{
    m_enableSync = value;
}
