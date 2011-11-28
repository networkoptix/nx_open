#include "syncplay_archive_delegate.h"
#include "syncplay_wrapper.h"
#include "abstract_archive_delegate.h"
#include "abstract_archive_stream_reader.h"
#include "utils/common/util.h"

QnSyncPlayArchiveDelegate::QnSyncPlayArchiveDelegate(QnAbstractArchiveReader* reader, QnArchiveSyncPlayWrapper* syncWrapper, 
                                                     QnAbstractArchiveDelegate* ownerDelegate):
    m_reader(reader),
    m_syncWrapper(syncWrapper), 
    m_ownerDelegate(ownerDelegate)
    //m_usePrebuffer(false)
{
    m_flags = ownerDelegate->getFlags();
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

void QnSyncPlayArchiveDelegate::beforeClose()
{
    m_ownerDelegate->beforeClose();
}

qint64 QnSyncPlayArchiveDelegate::startTime()
{
    return m_syncWrapper->minTime();
}

qint64 QnSyncPlayArchiveDelegate::endTime()
{
    return m_syncWrapper->endTime();
}

bool QnSyncPlayArchiveDelegate::isRealTimeSource() const 
{ 
    return m_ownerDelegate->isRealTimeSource();
}

void QnSyncPlayArchiveDelegate::setSingleshotMode(bool value)
{
    m_ownerDelegate->setSingleshotMode(value);
}

void QnSyncPlayArchiveDelegate::onReverseMode(qint64 displayTime, bool value)
{
    m_ownerDelegate->onReverseMode(displayTime, value);
}

/*
void QnSyncPlayArchiveDelegate::jumpToPreviousFrame (qint64 time)
{
    m_reader->jumpToPreviousFrame(time);
}

void QnSyncPlayArchiveDelegate::jumpTo (qint64 time)
{
    m_reader->jumpTo(time);
}
*/

qint64 QnSyncPlayArchiveDelegate::seek (qint64 time)
{
    //QMutexLocker lock(&m_mutex);
    //m_nextData.clear();
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

AVCodecContext* QnSyncPlayArchiveDelegate::setAudioChannel(int num)
{
    // play synchronized movies without audio
    return 0;
}

QnAbstractMediaDataPtr QnSyncPlayArchiveDelegate::getNextData()
{
    return m_ownerDelegate->getNextData();
    /*
    QMutexLocker lock(&m_mutex);
    QnAbstractMediaDataPtr rez;
    if (m_usePrebuffer)
    {
        rez = m_nextData ? m_nextData : m_ownerDelegate->getNextData();
        m_nextData = m_ownerDelegate->getNextData();
    }
    else 
    {
        rez = m_nextData ? m_nextData : m_ownerDelegate->getNextData();
        m_nextData.clear();
    }
    return rez;
    */
}

/*
void QnSyncPlayArchiveDelegate::setPrebuffering(bool value)
{
    QMutexLocker lock(&m_mutex);
    m_usePrebuffer = value;
    if (value && !m_nextData) 
        m_nextData = m_ownerDelegate->getNextData();
}
*/

/*
qint64 QnSyncPlayArchiveDelegate::getNextTime() const
{
    return m_nextData ? m_nextData->timestamp : AV_NOPTS_VALUE;
}
*/