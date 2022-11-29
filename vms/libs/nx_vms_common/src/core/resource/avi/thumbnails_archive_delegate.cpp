// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "thumbnails_archive_delegate.h"

#include <core/resource/media_resource.h>
#include <nx/utils/datetime.h>
#include <nx/utils/log/log.h>
#include <utils/common/util.h>

QnThumbnailsArchiveDelegate::QnThumbnailsArchiveDelegate(QnAbstractArchiveDelegatePtr baseDelegate):
    QnAbstractArchiveDelegate(),
    m_currentPos(AV_NOPTS_VALUE),
    m_rangeStart(AV_NOPTS_VALUE),
    m_rangeEnd(AV_NOPTS_VALUE),
    m_frameStep(0),
    m_lastMediaTime(0),
    m_baseDelegate(baseDelegate),
    m_nextChannelNum(0),
    m_channelCount(1)
{
}

void QnThumbnailsArchiveDelegate::setRange(qint64 startTime, qint64 endTime, qint64 frameStep)
{
    NX_VERBOSE(this,
        "Set range: %1 - %2, step %3 us",
        nx::utils::timestampToDebugString(startTime),
        nx::utils::timestampToDebugString(endTime),
        frameStep);

    m_frameStep = frameStep;
    m_currentPos = m_rangeStart = startTime;
    m_rangeEnd = endTime;
    m_baseDelegate->setRange(startTime, endTime, frameStep);
}

bool QnThumbnailsArchiveDelegate::open(const QnResourcePtr &resource,
    AbstractArchiveIntegrityWatcher * /*archiveIntegrityWatcher*/)

{
    m_lastMediaTime = 0;
    m_nextChannelNum = 0;
    QnMediaResourcePtr mediaRes = qSharedPointerDynamicCast<QnMediaResource>(resource);
    if (mediaRes)
    {
        if (QnConstResourceVideoLayoutPtr videoLayout = mediaRes->getVideoLayout(nullptr))
            m_channelCount = videoLayout->channelCount();
    }

    bool rez = m_baseDelegate->open(resource);
    if (rez)
        m_currentPos = m_rangeStart;
    else
        m_currentPos = AV_NOPTS_VALUE;
    return rez;
}

void QnThumbnailsArchiveDelegate::close()
{
    m_baseDelegate->close();
    m_currentPos = AV_NOPTS_VALUE;
}

qint64 QnThumbnailsArchiveDelegate::startTime() const
{
    return m_rangeStart;
}

qint64 QnThumbnailsArchiveDelegate::endTime() const
{
    return m_rangeEnd;
}

qint64 QnThumbnailsArchiveDelegate::seek (qint64 time, bool findIFrame)
{
    return m_baseDelegate->seek(time, findIFrame);
}

QnConstResourceVideoLayoutPtr QnThumbnailsArchiveDelegate::getVideoLayout()
{
    return m_baseDelegate->getVideoLayout();
}

AudioLayoutConstPtr QnThumbnailsArchiveDelegate::getAudioLayout()
{
    return m_baseDelegate->getAudioLayout();
}

QnAbstractMediaDataPtr QnThumbnailsArchiveDelegate::getNextData()
{
    NX_VERBOSE(this,
        "Get next frame. Range: %1 - %2, current pos %3, last frame %4",
        nx::utils::timestampToDebugString(m_rangeStart),
        nx::utils::timestampToDebugString(m_rangeEnd),
        nx::utils::timestampToDebugString(m_currentPos),
        nx::utils::timestampToDebugString(m_lastMediaTime));

    if (m_frameStep <= 0)
    {
        NX_WARNING(this, "Invalid frame step specified: %1", m_frameStep);
        return QnAbstractMediaDataPtr();
    }

    if (m_rangeStart == qint64(AV_NOPTS_VALUE))
        return QnAbstractMediaDataPtr();

    if (m_lastMediaTime > m_rangeEnd)
    {
        NX_VERBOSE(this, "Last frame is later than range end, stop.");
        return QnAbstractMediaDataPtr();
    }

    if (m_currentPos == DATETIME_NOW
        || m_rangeStart == DATETIME_NOW)
    {
        NX_VERBOSE(this, "Thumbnails from LIVE are not supported.");
        return QnAbstractMediaDataPtr();
    }

    bool delegateForMediaStep = m_baseDelegate->getFlags() & QnAbstractArchiveDelegate::Flag_CanProcessMediaStep;
    bool holeDetected = false;
    if (!delegateForMediaStep)
    {
        qint64 startTime = m_baseDelegate->startTime();
        if (startTime != qint64(AV_NOPTS_VALUE)) {
            qint64 outerSteps = (startTime - m_currentPos) / m_frameStep;
            if (outerSteps > 0)
                m_currentPos += outerSteps * m_frameStep;
        }

        qint64 seekRez = m_baseDelegate->seek(m_currentPos, true);
        if (seekRez == DATETIME_NOW)
            return QnAbstractMediaDataPtr(); //< End of archive reached.

        while (seekRez == -1 && m_currentPos < m_rangeEnd && m_rangeEnd != qint64(AV_NOPTS_VALUE))
        {
            m_currentPos += m_frameStep;
            seekRez = m_baseDelegate->seek(m_currentPos, true);
        }

        if (seekRez == -1)
            return QnAbstractMediaDataPtr();

        holeDetected = seekRez > m_currentPos;
        if (holeDetected)
            m_currentPos = seekRez - (seekRez-m_currentPos)%m_frameStep; //< align to grid
    }

    QnAbstractMediaDataPtr result;
    bool needToStop = false;
    while (!needToStop)
    {
        result = m_baseDelegate->getNextData();

        if (!result)
        {
            NX_VERBOSE(this, "Finish: frame read error.");
            needToStop = true;
        }
        else if (result->dataType == QnAbstractMediaData::EMPTY_DATA)
        {
            NX_VERBOSE(this, "Finish: EOF is reached.");
            needToStop = true;
        }
        else if (auto metadata = std::dynamic_pointer_cast<QnAbstractCompressedMetadata>(result);
            metadata && metadata->metadataType == MetadataType::MediaStreamEvent)
        {
            // Not enough licenses, archive is encrypted, etc.
            NX_VERBOSE(this, "Finish: media stream event has occured.");
            needToStop = true;
        }
        else if (result->dataType == QnAbstractMediaData::VIDEO &&
            result->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey) &&
            (int)result->channelNumber == m_nextChannelNum)
        {
            m_nextChannelNum = (m_nextChannelNum + 1) % m_channelCount;
            break;
        }
    }

    if (result)
    {
        if (!delegateForMediaStep)
        {
            if (holeDetected)
                result->flags |= QnAbstractMediaData::MediaFlags_BOF;
            else
                result->flags &= ~QnAbstractMediaData::MediaFlags_BOF;
        }
        m_lastMediaTime = result->timestamp;
    }

    if (!delegateForMediaStep)
        m_currentPos += m_frameStep;

    if (needToStop)
        m_currentPos = DATETIME_NOW;
    return result;
}

void QnThumbnailsArchiveDelegate::setGroupId(const nx::String& groupId)
{
    m_baseDelegate->setGroupId(groupId);
}

QnAbstractArchiveDelegate::ArchiveChunkInfo QnThumbnailsArchiveDelegate::getLastUsedChunkInfo() const
{
    return m_baseDelegate->getLastUsedChunkInfo();
}

CameraDiagnostics::Result QnThumbnailsArchiveDelegate::lastError() const
{
    return m_baseDelegate->lastError();
}
