#include "nov_archive_delegate.h"

#include "plugins/storage/file_storage/layout_storage_resource.h"
#include <utils/common/util.h>

QnNovArchiveDelegate::QnNovArchiveDelegate():
    QnAviArchiveDelegate(),
    m_skipFramesBeforeTime(AV_NOPTS_VALUE),
    m_reverseMode(false)
{

}

void QnNovArchiveDelegate::setSpeed(qint64 /*displayTime*/, double value)
{
    m_reverseMode = value < 0;
}

bool QnNovArchiveDelegate::open(const QnResourcePtr &resource,
    AbstractArchiveIntegrityWatcher * /*archiveIntegrityWatcher*/)
{
    m_skipFramesBeforeTime = AV_NOPTS_VALUE;

    m_chunks.clear();
    QnLayoutFileStorageResourcePtr layoutFile = m_storage.dynamicCast<QnLayoutFileStorageResource>();
    if (layoutFile)
        m_chunks = layoutFile->getTimePeriods(resource);

    return QnAviArchiveDelegate::open(resource);
}

qint64 QnNovArchiveDelegate::seek(qint64 time, bool findIFrame)
{
    m_skipFramesBeforeTime = AV_NOPTS_VALUE;
    if (!m_chunks.empty())  {
        qint64 oldTime = time;
        time = m_chunks.roundTimeToPeriodUSec(time, !m_reverseMode);

        if (!m_reverseMode && time != oldTime)
            m_skipFramesBeforeTime = time;
    }
    return QnAviArchiveDelegate::seek(time, findIFrame);
}

QnAbstractMediaDataPtr QnNovArchiveDelegate::getNextData()
{
    while (1)
    {
        QnAbstractMediaDataPtr data = QnAviArchiveDelegate::getNextData();

        if (data && m_skipFramesBeforeTime != (qint64)AV_NOPTS_VALUE)
        {
            if (data->timestamp < m_skipFramesBeforeTime)
                continue;
            else if (data->flags & AV_PKT_FLAG_KEY)
                m_skipFramesBeforeTime = AV_NOPTS_VALUE;
            else
                continue;
        }

        return data;
    }

    return QnAbstractMediaDataPtr();
}
