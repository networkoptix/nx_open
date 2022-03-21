// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "syncplay_archive_delegate.h"

#include <utils/common/util.h>

#include "syncplay_wrapper.h"

QnSyncPlayArchiveDelegate::QnSyncPlayArchiveDelegate(QnAbstractArchiveStreamReader* reader, QnArchiveSyncPlayWrapper* syncWrapper,
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
    if (m_syncWrapper)
        m_syncWrapper->erase(this);
    delete m_ownerDelegate;
}

bool QnSyncPlayArchiveDelegate::open(const QnResourcePtr &resource,
    AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher)
{
    return m_ownerDelegate->open(resource, archiveIntegrityWatcher);
}

void QnSyncPlayArchiveDelegate::close()
{
    m_ownerDelegate->close();
}

bool QnSyncPlayArchiveDelegate::reopen()
{
    return m_ownerDelegate->reopen();
}

void QnSyncPlayArchiveDelegate::beforeClose()
{
    m_ownerDelegate->beforeClose();
}

qint64 QnSyncPlayArchiveDelegate::startTime() const
{
    if (m_syncWrapper)
        return m_syncWrapper->minTime();
    return AV_NOPTS_VALUE;
}

qint64 QnSyncPlayArchiveDelegate::endTime() const
{
    if (m_syncWrapper)
        return m_syncWrapper->endTime();
    return AV_NOPTS_VALUE;
}

bool QnSyncPlayArchiveDelegate::isRealTimeSource() const
{
    return m_ownerDelegate->isRealTimeSource();
}

void QnSyncPlayArchiveDelegate::setSingleshotMode(bool value)
{
    m_ownerDelegate->setSingleshotMode(value);
}

void QnSyncPlayArchiveDelegate::setSpeed(qint64 displayTime, double value)
{
    m_ownerDelegate->setSpeed(displayTime, value);
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
    //NX_MUTEX_LOCKER lock( &m_mutex );
    //m_nextData.clear();
    return m_ownerDelegate->seek(time, findIFrame);
}

QnConstResourceVideoLayoutPtr QnSyncPlayArchiveDelegate::getVideoLayout()
{
    return m_ownerDelegate->getVideoLayout();
}

AudioLayoutConstPtr QnSyncPlayArchiveDelegate::getAudioLayout()
{
    return m_ownerDelegate->getAudioLayout();
}

bool QnSyncPlayArchiveDelegate::setAudioChannel(unsigned num)
{
    // play synchronized movies without audio // ??
    return m_ownerDelegate->setAudioChannel(num);
}

QnAbstractMediaDataPtr QnSyncPlayArchiveDelegate::getNextData()
{
    return m_ownerDelegate->getNextData();
}

void QnSyncPlayArchiveDelegate::beforeSeek(qint64 time)
{
    m_ownerDelegate->beforeSeek(time);
}

void QnSyncPlayArchiveDelegate::beforeChangeSpeed(double speed)
{
    m_ownerDelegate->beforeChangeSpeed(speed);
}

bool QnSyncPlayArchiveDelegate::setQuality(MediaQuality quality, bool fastSwitch, const QSize& size)
{
    return m_ownerDelegate->setQuality(quality, fastSwitch, size);
}

QnAbstractMotionArchiveConnectionPtr QnSyncPlayArchiveDelegate::getMotionConnection(int channel)
{
    return m_ownerDelegate->getMotionConnection(channel);
}

void QnSyncPlayArchiveDelegate::setStreamDataFilter(nx::vms::api::StreamDataFilters filter)
{
    m_ownerDelegate->setStreamDataFilter(filter);
}

nx::vms::api::StreamDataFilters QnSyncPlayArchiveDelegate::streamDataFilter() const
{
    return m_ownerDelegate->streamDataFilter();
}

void QnSyncPlayArchiveDelegate::setStorageLocationFilter(nx::vms::api::StorageLocation filter)
{
    m_ownerDelegate->setStorageLocationFilter(filter);
}

int QnSyncPlayArchiveDelegate::protocol() const
{
    return m_ownerDelegate->protocol();
}

QnAbstractArchiveDelegate::ArchiveChunkInfo QnSyncPlayArchiveDelegate::getLastUsedChunkInfo() const
{
    return m_ownerDelegate->getLastUsedChunkInfo();
}

bool QnSyncPlayArchiveDelegate::hasVideo() const
{
    return m_ownerDelegate->hasVideo();
}

void QnSyncPlayArchiveDelegate::pleaseStop()
{
    return m_ownerDelegate->pleaseStop();
}
