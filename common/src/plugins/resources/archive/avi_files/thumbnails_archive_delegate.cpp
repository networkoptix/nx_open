#include "thumbnails_archive_delegate.h"
#include "utils/common/util.h"

QnThumbnailsArchiveDelegate::QnThumbnailsArchiveDelegate(QnAbstractArchiveDelegatePtr baseDelegate):
    QnAbstractArchiveDelegate(),
    m_currentPos(AV_NOPTS_VALUE),
    m_rangeStart(AV_NOPTS_VALUE),
    m_rangeEnd(AV_NOPTS_VALUE),
    m_frameStep(0),
    m_baseDelegate(baseDelegate)
{
}

void QnThumbnailsArchiveDelegate::setRange(qint64 startTime, qint64 endTime, qint64 frameStep)
{
    m_frameStep = frameStep;
    m_currentPos = m_rangeStart = startTime;
    m_rangeEnd = endTime;
    m_baseDelegate->setRange(startTime, endTime, frameStep);
}

bool QnThumbnailsArchiveDelegate::open(QnResourcePtr resource)

{
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

qint64 QnThumbnailsArchiveDelegate::startTime()
{
    return m_rangeStart;
}

qint64 QnThumbnailsArchiveDelegate::endTime()
{
    return m_rangeEnd;
}

qint64 QnThumbnailsArchiveDelegate::seek (qint64 time, bool findIFrame)
{
    return m_baseDelegate->seek(time, findIFrame);
}

QnVideoResourceLayout* QnThumbnailsArchiveDelegate::getVideoLayout()
{
    return m_baseDelegate->getVideoLayout();
}

QnResourceAudioLayout* QnThumbnailsArchiveDelegate::getAudioLayout()
{
    return m_baseDelegate->getAudioLayout();
}

QnAbstractMediaDataPtr QnThumbnailsArchiveDelegate::getNextData()
{
    if (m_rangeStart == AV_NOPTS_VALUE)
        return QnAbstractMediaDataPtr();

    if (m_currentPos > m_rangeEnd || m_currentPos == DATETIME_NOW || m_rangeStart == DATETIME_NOW)
        return QnAbstractMediaDataPtr();

    bool delegateForMediaStep = m_baseDelegate->getFlags() & QnAbstractArchiveDelegate::Flag_CanProcessMediaStep;
    bool holeDetected = false;
    if (!delegateForMediaStep) 
    {
        qint64 seekRez = m_baseDelegate->seek(m_currentPos, true);
        holeDetected = seekRez > m_currentPos;
        m_currentPos = qMax(m_currentPos, seekRez);
    }
    
    QnAbstractMediaDataPtr result;
    do {
        result = m_baseDelegate->getNextData();
    }
    while (result && result->dataType != QnAbstractMediaData::VIDEO && result->dataType != QnAbstractMediaData::EMPTY_DATA);

    if (result) {
        if (holeDetected)
            result->flags |= QnAbstractMediaData::MediaFlags_BOF;
        else
            result->flags &= ~QnAbstractMediaData::MediaFlags_BOF;
    }

    if (!delegateForMediaStep)
        m_currentPos += m_frameStep;

    if (!result || result->dataType == QnAbstractMediaData::EMPTY_DATA)
        m_currentPos = DATETIME_NOW;
    return result;
}
