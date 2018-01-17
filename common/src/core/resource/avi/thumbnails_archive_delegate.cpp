#include "thumbnails_archive_delegate.h"

#include "utils/common/util.h"
#include <core/resource/media_resource.h>

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

    bool rez = m_baseDelegate->open(resource, nullptr);
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

QnConstResourceAudioLayoutPtr QnThumbnailsArchiveDelegate::getAudioLayout()
{
    return m_baseDelegate->getAudioLayout();
}

QnAbstractMediaDataPtr QnThumbnailsArchiveDelegate::getNextData()
{
    if (m_rangeStart == qint64(AV_NOPTS_VALUE))
        return QnAbstractMediaDataPtr();

    if (m_lastMediaTime > m_rangeEnd || m_currentPos == DATETIME_NOW || m_rangeStart == DATETIME_NOW)
        return QnAbstractMediaDataPtr();

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
        while (seekRez == -1 && m_currentPos < m_rangeEnd && m_rangeEnd != qint64(AV_NOPTS_VALUE)) {
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
    /*
    do {
        result = m_baseDelegate->getNextData();
    }
    while (result && result->dataType != QnAbstractMediaData::VIDEO && result->dataType != QnAbstractMediaData::EMPTY_DATA);
    */
    while (1)
    {
        result = m_baseDelegate->getNextData();
        if (!result || result->dataType == QnAbstractMediaData::EMPTY_DATA)
            break;
        if (result->dataType == QnAbstractMediaData::VIDEO &&
            result->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey) &&
            (int)result->channelNumber == m_nextChannelNum)
        {
            m_nextChannelNum = (m_nextChannelNum + 1) % m_channelCount;
            break;
        }

    }

    if (result) {
        if (!delegateForMediaStep) {
            if (holeDetected)
                result->flags |= QnAbstractMediaData::MediaFlags_BOF;
            else
                result->flags &= ~QnAbstractMediaData::MediaFlags_BOF;
        }
        m_lastMediaTime = result->timestamp;
    }

    if (!delegateForMediaStep)
        m_currentPos += m_frameStep;

    if (!result || result->dataType == QnAbstractMediaData::EMPTY_DATA)
        m_currentPos = DATETIME_NOW;
    return result;
}

void QnThumbnailsArchiveDelegate::setGroupId(const QByteArray& groupId)
{
    m_baseDelegate->setGroupId(groupId);
}

QnAbstractArchiveDelegate::ArchiveChunkInfo QnThumbnailsArchiveDelegate::getLastUsedChunkInfo() const
{
    return m_baseDelegate->getLastUsedChunkInfo();
}
