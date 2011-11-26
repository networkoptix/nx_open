#include "syncplay_archive_delegate.h"
#include "syncplay_wrapper.h"
#include "abstract_archive_delegate.h"
#include "abstract_archive_stream_reader.h"
#include "utils/common/util.h"

QnSyncPlayArchiveDelegate::QnSyncPlayArchiveDelegate(QnAbstractArchiveReader* reader, QnArchiveSyncPlayWrapper* syncWrapper, 
                                                     QnAbstractArchiveDelegate* ownerDelegate, QnlTimeSource* display):
    m_reader(reader),
    m_syncWrapper(syncWrapper), 
    m_ownerDelegate(ownerDelegate),
    m_display(display)
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

void QnSyncPlayArchiveDelegate::jumpToPreviousFrame (qint64 time, bool makeshot)
{
    QMutexLocker lock(&m_genericMutex);
    m_reader->jumpToPreviousFrame(time, makeshot);
}

void QnSyncPlayArchiveDelegate::jumpTo (qint64 time, bool makeshot)
{
    QMutexLocker lock(&m_genericMutex);
    m_reader->jumpTo(time, makeshot);
}


qint64 QnSyncPlayArchiveDelegate::seek (qint64 time)
{
    //return m_syncWrapper->seek(time);
    //m_seekTime = AV_NOPTS_VALUE;
    //m_tmpData.clear();
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

AVCodecContext* QnSyncPlayArchiveDelegate::setAudioChannel(int num)
{
    // play synchronized movies without audio
    return 0;
}

QnAbstractMediaDataPtr QnSyncPlayArchiveDelegate::getNextData()
{
    return m_ownerDelegate->getNextData();
}
