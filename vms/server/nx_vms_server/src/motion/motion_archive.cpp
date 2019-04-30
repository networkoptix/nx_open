#include "motion_archive.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include "utils/common/util.h"
#include "motion_helper.h"

#include <recording/time_period.h>
#include <recording/time_period_list.h>

#ifdef Q_OS_MAC
#include <smmintrin.h>
#endif

static const quint16 kAggregationIntervalSeconds = 3; // at seconds

// --------------- QnMotionArchiveConnection ---------------------

using namespace nx::vms::server::metadata;

QnMotionArchiveConnection::QnMotionArchiveConnection(QnMotionArchive* owner):
    m_owner(owner),
    m_minDate(AV_NOPTS_VALUE),
    m_maxDate(AV_NOPTS_VALUE),
    m_lastResult(new QnMetaDataV1())
{
    m_lastTimeMs = AV_NOPTS_VALUE;
    m_motionLoadedStart = m_motionLoadedEnd = -1;
    m_motionBuffer = (quint8*) qMallocAligned(1024 * owner->recordSize(), 32);
}

QnMotionArchiveConnection::~QnMotionArchiveConnection()
{
    qFreeAligned(m_motionBuffer);
}

QnAbstractCompressedMetadataPtr QnMotionArchiveConnection::getMotionData(qint64 timeUsec)
{
    if (m_lastResult->containTime(timeUsec))
        return QnMetaDataV1Ptr(); // do not duplicate data

    qint64 timeMs = timeUsec/1000;

    if (m_index.isEmpty() || timeMs < m_index.front().start + m_indexHeader.startTime ||
        timeMs > m_index.last().start + +m_index.last().duration + m_indexHeader.startTime)
    {
        qint64 prevMaxDate = m_maxDate;
        m_owner->dateBounds(timeMs, m_minDate, m_maxDate);
        if (prevMaxDate != m_maxDate)
        {
            m_lastTimeMs = AV_NOPTS_VALUE; // reset value. We are going to find iterator from vector begin() next time
            m_motionLoadedStart = m_motionLoadedEnd = -1;
            if (!m_owner->loadIndexFile(m_index, m_indexHeader, timeMs))
            {
                m_maxDate = AV_NOPTS_VALUE;
                return QnMetaDataV1Ptr();
            }
            m_maxDate = qMin(m_maxDate, QDateTime::currentDateTime().toMSecsSinceEpoch());
        }
    }

    if (m_lastTimeMs != (qint64)AV_NOPTS_VALUE && timeMs > m_lastTimeMs)
        m_indexItr = std::upper_bound(m_indexItr, m_index.end(), timeMs - m_indexHeader.startTime);
    else
        m_indexItr = std::upper_bound(m_index.begin(), m_index.end(), timeMs - m_indexHeader.startTime);

    if (m_indexItr > m_index.begin())
        m_indexItr--;

    qint64 foundStartTime = m_indexItr->start + m_indexHeader.startTime;
    bool isMotionFound = foundStartTime <= timeMs && foundStartTime + m_indexItr->duration > timeMs;
    if (!isMotionFound)
        return QnMetaDataV1Ptr();

    int indexOffset = m_indexItr - m_index.begin();
    bool isMotionLoaded = m_motionLoadedStart <= indexOffset && m_motionLoadedEnd > indexOffset;
    if (!isMotionLoaded)
    {
        m_owner->fillFileNames(timeMs, &m_motionFile, 0);
        if (m_motionFile.open(QFile::ReadOnly))
        {
            m_motionLoadedStart = qMax(0, indexOffset - 512);
            m_motionFile.seek(m_motionLoadedStart * m_owner->recordSize());
            int readed = qMax(0ll, m_motionFile.read((char*) m_motionBuffer, 1024 * m_owner->recordSize()));
            m_motionLoadedEnd = m_motionLoadedStart + readed / m_owner->recordSize();
            isMotionLoaded = m_motionLoadedStart <= indexOffset && m_motionLoadedEnd > indexOffset;
        }
    }
    if (!isMotionLoaded)
        return QnMetaDataV1Ptr();


    m_lastResult = QnMetaDataV1Ptr(new QnMetaDataV1());
    m_lastResult->m_data.clear();
    int motionIndex = indexOffset - m_motionLoadedStart;
    m_lastResult->m_data.write((const char*) m_motionBuffer + motionIndex * m_owner->recordSize(), m_owner->recordSize());
    m_lastResult->timestamp = foundStartTime*1000;
    m_lastResult->m_duration = m_indexItr->duration*1000;
    m_lastResult->channelNumber = m_owner->getChannel();

    //qDebug() << "motion=" << QDateTime::fromMSecsSinceEpoch(m_lastResult->timestamp/1000).toString("hh.mm.ss.zzz")
    //         << "dur=" << m_lastResult->m_duration/1000.0
    //         <<  "reqTime=" << QDateTime::fromMSecsSinceEpoch(timeMs).toString("hh.mm.ss.zzz");

    return m_lastResult;
}

// ----------------------- QnMotionArchive ------------------

QnMotionArchive::QnMotionArchive(
    const QString& dataDir,
    const QString& uniqueId,
    int channel)
    :
    MetadataArchive(
        "motion",
        kMotionDataRecordSize,
        kAggregationIntervalSeconds,
        dataDir,
        uniqueId,
        channel),
    m_lastDetailedData(new QnMetaDataV1()),
    m_lastTimestamp(AV_NOPTS_VALUE)
{
}

QnMotionArchive::~QnMotionArchive()
{
}

QnMotionArchiveConnectionPtr QnMotionArchive::createConnection()
{
    return QnMotionArchiveConnectionPtr(new QnMotionArchiveConnection(this));
}

bool QnMotionArchive::saveToArchive(QnConstMetaDataV1Ptr data)
{
    bool rez = true;

    if (m_lastTimestamp == (qint64)AV_NOPTS_VALUE) {
        m_lastDetailedData->m_duration = 0;
        m_lastDetailedData->timestamp = data->timestamp;
    }
    else {
        if (data->timestamp >= m_lastTimestamp && data->timestamp - m_lastTimestamp <= MAX_FRAME_DURATION_MS * 1000ll) {
            m_lastDetailedData->m_duration = data->timestamp - m_lastDetailedData->timestamp;
        }
        else {
            ; // close motion period at previous packet
        }
    }

    if (data->timestamp >= m_lastDetailedData->timestamp && data->timestamp - m_lastDetailedData->timestamp < aggregationIntervalSeconds() * 1000000ll)
    {
        //qDebug() << "addMotion=" << QDateTime::fromMSecsSinceEpoch(data->timestamp/1000).toString("hh.mm.ss.zzz")
        //    << "to" << QDateTime::fromMSecsSinceEpoch(m_lastDetailedData->timestamp).toString("hh.mm.ss.zzz");

        // aggregate data
        m_lastDetailedData->addMotion(data);
    }
    else {
        // save to disk
        if (!m_lastDetailedData->isEmpty())
            rez = saveToArchiveInternal(m_lastDetailedData);
        m_lastDetailedData = QnMetaDataV1Ptr(data->clone());
        m_lastDetailedData->m_duration = 0;
        //qDebug() << "start new Motion" << QDateTime::fromMSecsSinceEpoch(data->timestamp/1000).toString("hh.mm.ss.zzz");
    }
    m_lastTimestamp = data->timestamp;
    return rez;
}

