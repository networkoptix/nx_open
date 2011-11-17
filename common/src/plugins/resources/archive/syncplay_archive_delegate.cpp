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

qint64 QnSyncPlayArchiveDelegate::jumpTo (qint64 time, bool makeshot)
{
    QMutexLocker lock(&m_genericMutex);
    m_seekTime = time;
    m_reader->jumpTo(time, makeshot);
    return time;
}

qint64 QnSyncPlayArchiveDelegate::seek (qint64 time)
{
    //return m_syncWrapper->seek(time);
    m_seekTime = AV_NOPTS_VALUE;
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
    if (m_tmpData && m_seekTime == AV_NOPTS_VALUE)
        return m_tmpData->timestamp;
    else
        return AV_NOPTS_VALUE;
}

QnAbstractMediaDataPtr QnSyncPlayArchiveDelegate::getNextData()
{
    {
        QMutexLocker lock(&m_genericMutex);
        if (m_seekTime != AV_NOPTS_VALUE) 
        {
            m_tmpData = QnAbstractMediaDataPtr(0);
            //m_ownerDelegate->seek(m_seekTime);
            m_seekTime = AV_NOPTS_VALUE;
        }
    }

    QnAbstractMediaDataPtr readedData = m_ownerDelegate->getNextData();
    bool isPrimaryVideo = qSharedPointerDynamicCast<QnCompressedVideoData>(readedData) && readedData->channelNumber == 0;
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
    if (m_tmpData && m_enableSync && !m_liveMode)
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
