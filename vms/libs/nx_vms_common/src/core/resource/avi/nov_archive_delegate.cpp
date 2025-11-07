// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nov_archive_delegate.h"

#include <core/storage/file_storage/layout_storage_resource.h>
#include <nx/utils/fs/file.h>
#include <nx/utils/log/log.h>
#include <utils/common/util.h>

QnNovArchiveDelegate::QnNovArchiveDelegate(const QnStorageResourcePtr& storage):
    m_storage(storage),
    m_skipFramesBeforeTime(AV_NOPTS_VALUE),
    m_reverseMode(false)
{
    m_currentFile.setStorage(storage);
}

void QnNovArchiveDelegate::setSpeed(qint64 /*displayTime*/, double value)
{
    m_reverseMode = value < 0;
}

bool QnNovArchiveDelegate::hasVideo() const
{
    return m_currentFile.hasVideo();
}

QString QnNovArchiveDelegate::getFileName(const QString &url, int64_t timestamp) const
{
    auto filename = url;
    filename.replace(".mkv", "_" + QString::number(timestamp) + ".mkv");
    return filename;
}

bool QnNovArchiveDelegate::open(const QnResourcePtr &resource,
    AbstractArchiveIntegrityWatcher * /*archiveIntegrityWatcher*/)
{
    // TODO fix it, Archive stream reader should not call this func(open) between subsequent
    // calls getNextData.
    if (m_currentFile.opened())
        return true;

    m_resource = resource;
    m_skipFramesBeforeTime = AV_NOPTS_VALUE;

    m_chunks.clear();
    QnLayoutFileStorageResourcePtr layoutFile =
        m_storage.dynamicCast<QnLayoutFileStorageResource>();
    if (layoutFile)
    {
        m_chunks = layoutFile->getTimePeriods(resource);
        m_startTimes = layoutFile->getStartTimes(resource);
    }
    m_fileIndex = 0;
    QString filename;
    if (!m_startTimes.empty())
        filename = getFileName(resource->getUrl(), m_startTimes[0]);
    else
        filename = resource->getUrl();

    if (!openFile(filename))
        return false;

    findStartEndTime(resource);

    // For multifile nov archive metadata have to be saved in first file.
    m_metadata = m_currentFile.metadata();
    return true;
}

void QnNovArchiveDelegate::findStartEndTime(const QnResourcePtr& resource)
{
    if (m_startTimes.size() <= 1)
    {
        m_startTime = m_currentFile.startTime();
        m_endTime = m_currentFile.endTime();
        return;
    }
    QnAviArchiveDelegate delegate;
    delegate.setStorage(m_storage);
    if (delegate.open(resource, getFileName(resource->getUrl(), m_startTimes[0])))
        m_startTime = delegate.startTime();

    delegate.close();
    if (delegate.open(resource, getFileName(resource->getUrl(), m_startTimes.back())))
    {
        delegate.setStartTimeUs(m_startTimes.back() * 1000);
        m_endTime = delegate.endTime();
    }
}

bool QnNovArchiveDelegate::setAudioChannel(unsigned num)
{
    m_audioChannel = num;
    return m_currentFile.setAudioChannel(m_audioChannel);
}

qint64 QnNovArchiveDelegate::startTime() const
{
    return m_startTime;
}

qint64 QnNovArchiveDelegate::endTime() const
{
    return m_endTime;
}

std::optional<QnAviArchiveMetadata> QnNovArchiveDelegate::metadata() const
{
    return m_metadata;
}

QnAbstractMotionArchiveConnectionPtr QnNovArchiveDelegate::getMotionConnection(int channel)
{
    return m_currentFile.getMotionConnection(channel);
}

bool QnNovArchiveDelegate::providesMotionPackets() const
{
    if (!m_metadata)
        return false;

    return m_metadata->metadataStreamVersion >= QnAviArchiveMetadata::kMetadataStreamVersion_2;
}

bool QnNovArchiveDelegate::reopen()
{
    return m_currentFile.reopen();
}

qint64 QnNovArchiveDelegate::seek(qint64 time, bool findIFrame)
{
    NX_VERBOSE(this, "Seek to: %1 us", time);
    m_skipFramesBeforeTime = AV_NOPTS_VALUE;
    if (!m_chunks.empty())  {
        qint64 oldTime = time;
        time = m_chunks.roundTimeToPeriodUSec(time, !m_reverseMode);
        NX_VERBOSE(this, "Seek position is rounded up to: %1 us", time);

        if (!m_reverseMode && time != oldTime)
            m_skipFramesBeforeTime = time;
    }

    if (!m_startTimes.empty())
    {
        int fileIndex = 0;
        for (; fileIndex < int(m_startTimes.size() - 1); ++fileIndex)
        {
            if (m_startTimes[fileIndex + 1] > time / 1000)
                break;
        }
        if (fileIndex != m_fileIndex)
        {
            m_fileIndex = fileIndex;
            auto filename = getFileName(m_resource->getUrl(), m_startTimes[m_fileIndex]);
            NX_VERBOSE(this, "Seek to file: %1", filename);
            openFile(filename);
        }
    }

    if (time != DATETIME_NOW)
        time = std::min(m_currentFile.endTime(), time);
    return m_currentFile.seek(time, findIFrame);
}

bool QnNovArchiveDelegate::openFile(const QString& filename)
{
    NX_DEBUG(this, "Open file: %1", filename);
    m_currentFile.close();
    m_currentFile.setStorage(m_storage);
    m_currentFile.setAudioChannel(m_audioChannel);

    if (!m_currentFile.open(m_resource, filename))
        return false;

    if (!m_startTimes.empty() && m_fileIndex < (int)m_startTimes.size())
        m_currentFile.setStartTimeUs(m_startTimes[m_fileIndex] * 1000);

    return true;
}

QnConstResourceVideoLayoutPtr QnNovArchiveDelegate::getVideoLayout()
{
    return m_currentFile.getVideoLayout();
}

AudioLayoutConstPtr QnNovArchiveDelegate::getAudioLayout()
{
    return m_currentFile.getAudioLayout();
}

void QnNovArchiveDelegate::close()
{
    return m_currentFile.close();
}

QnAbstractMediaDataPtr QnNovArchiveDelegate::getNextData()
{
    while (1)
    {
        QnAbstractMediaDataPtr data = m_currentFile.getNextData();
        if (!data)
        {
            m_fileIndex++;
            if (m_fileIndex < (int)m_startTimes.size())
            {
                auto filename = getFileName(m_resource->getUrl(), m_startTimes[m_fileIndex]);
                if (!openFile(filename))
                    return data;

                data = m_currentFile.getNextData();
            }

        }

        if (data && m_skipFramesBeforeTime != (qint64)AV_NOPTS_VALUE)
        {
            if (data->timestamp < m_skipFramesBeforeTime)
                continue;
            else if (data->flags & AV_PKT_FLAG_KEY)
                m_skipFramesBeforeTime = AV_NOPTS_VALUE;
            else
                continue;
        }
        NX_VERBOSE(this, "Next data: %1", data);
        return data;
    }

    return QnAbstractMediaDataPtr();
}
