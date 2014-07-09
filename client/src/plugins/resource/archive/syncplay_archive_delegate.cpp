#include "syncplay_archive_delegate.h"

#include <utils/common/util.h>

#include "syncplay_wrapper.h"

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
    m_syncWrapper->erase(this);
    delete m_ownerDelegate;
}

bool QnSyncPlayArchiveDelegate::open(const QnResourcePtr &resource)
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

qint64 QnSyncPlayArchiveDelegate::seek (qint64 time, bool findIFrame)
{
    //QMutexLocker lock(&m_mutex);
    //m_nextData.clear();
    return m_ownerDelegate->seek(time, findIFrame);
}

QnResourceVideoLayoutPtr QnSyncPlayArchiveDelegate::getVideoLayout()
{
    return m_ownerDelegate->getVideoLayout();
}

QnResourceAudioLayoutPtr QnSyncPlayArchiveDelegate::getAudioLayout()
{
    return m_ownerDelegate->getAudioLayout();
}

AVCodecContext* QnSyncPlayArchiveDelegate::setAudioChannel(int num)
{
    // play synchronized movies without audio // ??
    m_ownerDelegate->setAudioChannel(num);
    return 0;
}

QnAbstractMediaDataPtr QnSyncPlayArchiveDelegate::getNextData()
{
    return m_ownerDelegate->getNextData();
}

/*
void QnSyncPlayArchiveDelegate::setMotionRegion(const QRegion& region)
{
    QnAbstractFilterPlaybackDelegate* maskedDelegate = dynamic_cast<QnAbstractFilterPlaybackDelegate*>(m_ownerDelegate);
    if (maskedDelegate)
        maskedDelegate->setMotionRegion(region);
}
void QnSyncPlayArchiveDelegate::setSendMotion(bool value)
{
    QnAbstractFilterPlaybackDelegate* maskedDelegate = dynamic_cast<QnAbstractFilterPlaybackDelegate*>(m_ownerDelegate);
    //if (maskedDelegate)
    //    m_ownerDelegate->setSendMotion(value);
}
*/

void QnSyncPlayArchiveDelegate::beforeSeek(qint64 time)
{
    m_ownerDelegate->beforeSeek(time);
}

void QnSyncPlayArchiveDelegate::beforeChangeReverseMode(bool reverseMode)
{
    m_ownerDelegate->beforeChangeReverseMode(reverseMode);
}

bool QnSyncPlayArchiveDelegate::setQuality(MediaQuality quality, bool fastSwitch)
{
    return m_ownerDelegate->setQuality(quality, fastSwitch);
}

QnAbstractMotionArchiveConnectionPtr QnSyncPlayArchiveDelegate::getMotionConnection(int channel)
{
    return m_ownerDelegate->getMotionConnection(channel);
}

void QnSyncPlayArchiveDelegate::setSendMotion(bool value)
{
    m_ownerDelegate->setSendMotion(value);
}

QnAbstractArchiveDelegate::ArchiveChunkInfo QnSyncPlayArchiveDelegate::getLastUsedChunkInfo() const
{
    return m_ownerDelegate->getLastUsedChunkInfo();
}
