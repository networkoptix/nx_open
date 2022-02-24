// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "archive_delegate_wrapper.h"

namespace nx {
namespace vms::server {
namespace plugins {

ArchiveDelegateWrapper::ArchiveDelegateWrapper(std::unique_ptr<QnAbstractArchiveDelegate> delegate):
    m_delegate(std::move(delegate))
{
}

bool ArchiveDelegateWrapper::open(
    const QnResourcePtr &resource,
    AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher)
{
    return m_delegate->open(resource, archiveIntegrityWatcher);
}

void ArchiveDelegateWrapper::close()
{
    m_delegate->close();
}

qint64 ArchiveDelegateWrapper::startTime() const
{
    return m_delegate->startTime();
}

qint64 ArchiveDelegateWrapper::endTime() const
{
    return m_delegate->endTime();
}

QnAbstractMediaDataPtr ArchiveDelegateWrapper::getNextData()
{
    return m_delegate->getNextData();
}

qint64 ArchiveDelegateWrapper::seek(qint64 time, bool findIframe)
{
    return m_delegate->seek(time, findIframe);
}

QnConstResourceVideoLayoutPtr ArchiveDelegateWrapper::getVideoLayout()
{
    return m_delegate->getVideoLayout();
}

AudioLayoutConstPtr ArchiveDelegateWrapper::getAudioLayout()
{
    return m_delegate->getAudioLayout();
}

bool ArchiveDelegateWrapper::hasVideo() const
{
    return m_delegate->hasVideo();
}

bool ArchiveDelegateWrapper::setAudioChannel(unsigned num)
{
    return m_delegate->setAudioChannel(num);
}

void ArchiveDelegateWrapper::setSpeed(qint64 displayTime, double value)
{
    m_delegate->setSpeed(displayTime, value);
}

void ArchiveDelegateWrapper::setSingleshotMode(bool value)
{
    m_delegate->setSingleshotMode(value);
}

bool ArchiveDelegateWrapper::setQuality(MediaQuality quality, bool fastSwitch, const QSize& resolution)
{
    return m_delegate->setQuality(quality, fastSwitch, resolution);
}

bool ArchiveDelegateWrapper::isRealTimeSource() const
{
    return m_delegate->isRealTimeSource();
}

void ArchiveDelegateWrapper::beforeClose()
{
    m_delegate->beforeClose();
}

void ArchiveDelegateWrapper::beforeSeek(qint64 time)
{
    m_delegate->beforeSeek(time);
}

void ArchiveDelegateWrapper::beforeChangeSpeed(double speed)
{
    m_delegate->beforeChangeSpeed(speed);
}

void ArchiveDelegateWrapper::setRange(qint64 startTime, qint64 endTime, qint64 frameStep)
{
    m_delegate->setRange(startTime, endTime, frameStep);
}

QnAbstractMotionArchiveConnectionPtr ArchiveDelegateWrapper::getMotionConnection(int channel)
{
    return m_delegate->getMotionConnection(channel);
}

void ArchiveDelegateWrapper::setStreamDataFilter(nx::vms::api::StreamDataFilters filter)
{
    m_delegate->setStreamDataFilter(filter);
}

nx::vms::api::StreamDataFilters ArchiveDelegateWrapper::streamDataFilter() const
{
    return m_delegate->streamDataFilter();
}

void ArchiveDelegateWrapper::setStorageLocationFilter(nx::vms::api::StorageLocation filter)
{
    m_delegate->setStorageLocationFilter(filter);
}

void ArchiveDelegateWrapper::setMotionRegion(const QnMotionRegion& region)
{
    m_delegate->setMotionRegion(region);
}

void ArchiveDelegateWrapper::setGroupId(const nx::String& groupId)
{
    m_delegate->setGroupId(groupId);
}

QnAbstractArchiveDelegate::ArchiveChunkInfo ArchiveDelegateWrapper::getLastUsedChunkInfo() const
{
    return m_delegate->getLastUsedChunkInfo();
}

int ArchiveDelegateWrapper::getSequence() const
{
    return m_delegate->getSequence();
}

void ArchiveDelegateWrapper::setPlaybackMode(PlaybackMode value)
{
    m_delegate->setPlaybackMode(value);
}

void ArchiveDelegateWrapper::setEndOfPlaybackHandler(std::function<void()> handler)
{
    m_delegate->setEndOfPlaybackHandler(handler);
}

void ArchiveDelegateWrapper::setErrorHandler(std::function<void(const QString& errorString)> handler)
{
    m_delegate->setErrorHandler(handler);
}

QnAbstractArchiveDelegate* ArchiveDelegateWrapper::delegate() const
{
    return m_delegate.get();
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
