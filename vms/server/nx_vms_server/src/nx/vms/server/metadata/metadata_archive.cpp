#include "metadata_archive.h"

#include <QtCore/QDir>

#include <nx/vms/server/metadata/metadata_helper.h>
#include "utils/media/sse_helper.h"
#include <nx/utils/scope_guard.h>
#include <nx/utils/log/log_main.h>

namespace nx::vms::server::metadata {

static const char kVersion = 1;
static const int kRecordsPerIteration = 1024;

MetadataArchive::MetadataArchive(
    const QString& filePrefix,
    int recordSize,
    int aggregationIntervalSeconds,
    const QString& dataDir,
    const QString& uniqueId,
    int channel)
    :
    m_filePrefix(filePrefix),
    m_recordSize(recordSize),
    m_aggregationIntervalSeconds(aggregationIntervalSeconds),
    m_dataDir(dataDir),
    m_uniqueId(uniqueId),
    m_channel(channel)
{
    loadRecordedRange();
}

int MetadataArchive::recordSize() const
{
    return m_recordSize;
}

int MetadataArchive::aggregationIntervalSeconds() const
{
    return m_aggregationIntervalSeconds;
}

void MetadataArchive::loadRecordedRange()
{
    m_minMetadataTime = AV_NOPTS_VALUE;
    m_maxMetadataTime = AV_NOPTS_VALUE;
    m_lastRecordedTime = AV_NOPTS_VALUE;
    MetadataHelper helper(m_dataDir);
    QList<QDate> existsRecords = helper.recordedMonth(m_uniqueId);
    if (existsRecords.isEmpty())
        return;

    QVector<IndexRecord> index;
    IndexHeader indexHeader;
    loadIndexFile(index, indexHeader, existsRecords.first());
    if (!index.isEmpty())
        m_minMetadataTime = index.first().start + indexHeader.startTime;
    if (existsRecords.size() > 1)
        loadIndexFile(index, indexHeader, existsRecords.last());
    if (!index.isEmpty())
        m_lastRecordedTime = m_maxMetadataTime = index.last().start + indexHeader.startTime;
}

QString MetadataArchive::getFilePrefix(const QDate& datetime) const
{
    MetadataHelper helper(m_dataDir);
    return helper.getDirByDateTime(datetime, m_uniqueId);
}

QString MetadataArchive::getChannelPrefix() const
{
    return m_channel == 0 ? QString() : QString::number(m_channel);
}

void MetadataArchive::fillFileNames(qint64 datetimeMs, QFile* metadataFile, QFile* indexFile) const
{
    QDateTime datetime = QDateTime::fromMSecsSinceEpoch(datetimeMs);
    QString fileName = getFilePrefix(datetime.date());
    if (metadataFile)
    {
        metadataFile->close();
        metadataFile->setFileName(fileName + QString("%1_detailed_data").arg(m_filePrefix) + getChannelPrefix() + ".bin");
    }
    if (indexFile)
    {
        indexFile->close();
        indexFile->setFileName(fileName + QString("%1_detailed_index").arg(m_filePrefix) + getChannelPrefix() + ".bin");
    }
}

bool MetadataArchive::saveToArchiveInternal(const QnAbstractCompressedMetadataPtr& data)
{
    QnMutexLocker lock(&m_writeMutex);

    const qint64 timestamp = data->timestamp / 1000;
    if (timestamp > m_lastDateForCurrentFile || timestamp < m_firstTime)
    {
        // go to new file

        m_middleRecordNum = -1;
        m_index.clear();
        dateBounds(timestamp, m_firstTime, m_lastDateForCurrentFile);

        //QString fileName = getFilePrefix(datetime);
        fillFileNames(timestamp, &m_detailedMetadataFile, &m_detailedIndexFile);

        auto dirName = getFilePrefix(QDateTime::fromMSecsSinceEpoch(timestamp).date());
        QDir dir;
        dir.mkpath(dirName);

        if (!m_detailedMetadataFile.open(QFile::ReadWrite))
            return false;

        if (!m_detailedIndexFile.open(QFile::ReadWrite))
            return false;

        // truncate biggest file. So, it is error checking
        int indexRecords = qMax((m_detailedIndexFile.size() - kMetadataIndexHeaderSize) / kIndexRecordSize, 0ll);
        int dataRecords = m_detailedMetadataFile.size() / recordSize();
        if (indexRecords > dataRecords)
        {
            if (!m_detailedIndexFile.resize(dataRecords*kIndexRecordSize + kMetadataIndexHeaderSize))
                return false;
        }
        else if ((indexRecords < dataRecords))
        {
            if (!m_detailedMetadataFile.resize(indexRecords * recordSize()))
                return false;
        }

        if (m_detailedIndexFile.size() == 0)
        {
            IndexHeader header;
            header.version = kVersion;
            header.interval = m_aggregationIntervalSeconds;
            header.startTime = m_firstTime;
            m_detailedIndexFile.write((const char*)&header, sizeof(IndexHeader));
        }
        else {
            loadIndexFile(m_index, m_indexHeader, m_detailedIndexFile);
            m_firstTime = m_indexHeader.startTime;
            if (m_index.size() > 0) {
                m_lastRecordedTime = m_maxMetadataTime = m_index.last().start + m_indexHeader.startTime;
            }
        }
        m_detailedMetadataFile.seek(m_detailedMetadataFile.size());
        m_detailedIndexFile.seek(m_detailedIndexFile.size());
    }

    if (timestamp < m_lastRecordedTime)
    {
        // go to the file middle
        m_middleRecordNum = getSizeForTime(timestamp, true);
        m_detailedIndexFile.seek(m_middleRecordNum*kIndexRecordSize + kMetadataIndexHeaderSize);
        m_detailedMetadataFile.seek(m_middleRecordNum * recordSize());
        m_minMetadataTime = qMin(m_minMetadataTime.load(), timestamp);
    }
    else if (m_middleRecordNum >= 0 && m_middleRecordNum < m_index.size())
    {
        qint64 timeInIndex = m_index[m_middleRecordNum].start + m_indexHeader.startTime;
        if (timestamp > timeInIndex) {
            m_middleRecordNum = getSizeForTime(timestamp, false);
            m_detailedIndexFile.seek(m_middleRecordNum*kIndexRecordSize + kMetadataIndexHeaderSize);
            m_detailedMetadataFile.seek(m_middleRecordNum * recordSize());
        }
    }

    /**
     * It is possible that timestamp < m_firstTime in case of time has been changed to the past
     * and timezone (or DST flag) has been changed at the same time.
     */
    quint32 relTime = quint32(qMax(0LL, timestamp - m_firstTime));
    quint32 duration = std::max(kMinimalDurationMs, quint32(data->m_duration / 1000));
    if (m_detailedIndexFile.write((const char*)&relTime, 4) != 4)
    {
        qWarning() << "Failed to write index file for camera" << m_uniqueId;
    }
    if (m_detailedIndexFile.write((const char*)&duration, 4) != 4)
    {
        qWarning() << "Failed to write index file for camera" << m_uniqueId;
    }

    if (m_detailedMetadataFile.write(data->data(), data->dataSize())
        != static_cast<qint64>(data->dataSize()))
    {
        qWarning() << "Failed to write index file for camera" << m_uniqueId;
    }

    m_detailedIndexFile.flush();
    m_detailedMetadataFile.flush();

    m_lastRecordedTime = timestamp;
    m_maxMetadataTime = qMax(m_maxMetadataTime.load(), timestamp);
    if (m_minMetadataTime == (qint64)AV_NOPTS_VALUE)
        m_minMetadataTime = m_maxMetadataTime.load();
    m_middleRecordNum++;
    return true;
}

bool MetadataArchive::loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, const QDateTime& time) const
{
    return loadIndexFile(index, indexHeader, time.toMSecsSinceEpoch());
}

bool MetadataArchive::loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, const QDate& time) const
{
    return loadIndexFile(index, indexHeader, QDateTime(time).toMSecsSinceEpoch());
}

bool MetadataArchive::loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, qint64 msTime) const
{
    QFile indexFile;
    fillFileNames(msTime, 0, &indexFile);
    if (!indexFile.open(QFile::ReadOnly))
        return false;
    return loadIndexFile(index, indexHeader, indexFile);
}

bool MetadataArchive::loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, QFile& indexFile) const
{
    static const size_t READ_BUF_SIZE = 128 * 1024;

    index.clear();
    indexFile.seek(0);
    indexFile.read((char*)&indexHeader, kMetadataIndexHeaderSize);

    std::vector<quint8> tmpBuffer(READ_BUF_SIZE);
    while (1)
    {
        int readed = indexFile.read((char*)&tmpBuffer[0], READ_BUF_SIZE);
        if (readed < 1)
            break;
        int records = readed / kIndexRecordSize;
        int oldVectorSize = index.size();
        index.resize(oldVectorSize + records);
        memcpy(&index[oldVectorSize], &tmpBuffer[0], records * kIndexRecordSize);
    }

    return true;
}

void MetadataArchive::dateBounds(qint64 datetimeMs, qint64& minDate, qint64& maxDate) const
{
    QDateTime datetime = QDateTime::fromMSecsSinceEpoch(datetimeMs);
    QDateTime monthStart(datetime.date().addDays(1 - datetime.date().day()));
    minDate = monthStart.toMSecsSinceEpoch();

    QDateTime nextMonth(monthStart.addMonths(1));
    maxDate = nextMonth.toMSecsSinceEpoch() - 1;
}

int MetadataArchive::getSizeForTime(qint64 timeMs, bool reloadIndex)
{
    if (reloadIndex)
        loadIndexFile(m_index, m_indexHeader, m_detailedIndexFile);
    QVector<IndexRecord>::iterator indexIterator = std::lower_bound(m_index.begin(), m_index.end(), timeMs - m_indexHeader.startTime);
    return indexIterator - m_index.begin();
}

void MetadataArchive::loadDataFromIndex(
    MatchExtraDataFunc matchExtraData,
    std::function<bool()> interruptionCallback,
    const Filter& filter,
    QFile& metadataFile,
    const IndexHeader& indexHeader,
    const QVector<IndexRecord>& index,
    QVector<IndexRecord>::iterator startItr,
    QVector<IndexRecord>::iterator endItr,
    quint8* buffer,
    simd128i* mask,
    int maskStart,
    int maskEnd,
    QnTimePeriodList& rez)
{
    int totalSteps = endItr - startItr;

    int recordNumberToSeek = startItr - index.begin();
    metadataFile.seek(recordNumberToSeek * recordSize());

    // math file (one month)
    QVector<IndexRecord>::const_iterator i = startItr;
    while (totalSteps > 0)
    {
        int readed = metadataFile.read((char*)buffer, recordSize() * qMin(totalSteps, kRecordsPerIteration));
        if (readed <= 0)
            break;
        quint8* dataEnd = buffer + readed;
        quint8* curData = buffer;
        while (i < endItr && curData < dataEnd)
        {
            qint64 fullStartTime = i->start + indexHeader.startTime;
            if (QnMetaDataV1::matchImage((quint64*)curData, mask, maskStart, maskEnd)
                && (!matchExtraData
                    || matchExtraData(fullStartTime, filter, curData + kGridDataSizeBytes, recordSize() - kGridDataSizeBytes)))
            {
                if (rez.empty())
                {
                    rez.push_back(QnTimePeriod(fullStartTime, i->duration));
                }
                else
                {
                    QnTimePeriod& last = rez.back();
                    if (fullStartTime <= last.startTimeMs + last.durationMs + filter.detailLevel.count())
                    {
                        last.durationMs = qMax(last.durationMs, i->duration + fullStartTime - last.startTimeMs);
                    }
                    else
                    {
                        if (rez.size() == filter.limit)
                            return;
                        rez.push_back(QnTimePeriod(fullStartTime, i->duration));
                    }
                }
            }
            if (interruptionCallback && interruptionCallback())
                return;

            curData += recordSize();
            ++i;
        }
        totalSteps -= readed / recordSize();
    }
}

void MetadataArchive::loadDataFromIndexDesc(
    MatchExtraDataFunc matchExtraData,
    std::function<bool()> interruptionCallback,
    const Filter& filter,
    QFile& metadataFile,
    const IndexHeader& indexHeader,
    const QVector<IndexRecord>& index,
    QVector<IndexRecord>::iterator startItr,
    QVector<IndexRecord>::iterator endItr,
    quint8* buffer,
    simd128i* mask,
    int maskStart,
    int maskEnd,
    QnTimePeriodList& rez)
{
    int totalSteps = endItr - startItr;

    int recordNumberToSeek = qMax<int>(0LL, (endItr - std::min(kRecordsPerIteration, totalSteps)) - index.begin());

    // math file (one month)
    QVector<IndexRecord>::const_iterator i = endItr - 1;
    while (totalSteps > 0)
    {
        metadataFile.seek(recordNumberToSeek * recordSize());
        int readed = metadataFile.read(
            (char*)buffer, recordSize() * qMin(totalSteps, kRecordsPerIteration));
        if (readed <= 0)
            break;

        quint8* dataEnd = buffer + readed;
        quint8* curData = dataEnd - recordSize();
        while (i >= startItr && curData >= buffer)
        {
            qint64 fullStartTimeMs = i->start + indexHeader.startTime;
            if (QnMetaDataV1::matchImage((quint64*)curData, mask, maskStart, maskEnd)
                && (!matchExtraData ||
                    matchExtraData(fullStartTimeMs, filter, curData + kGridDataSizeBytes, recordSize() - kGridDataSizeBytes)))
            {
                if (rez.empty())
                {
                    rez.push_back(QnTimePeriod(fullStartTimeMs, i->duration));
                }
                else
                {
                    QnTimePeriod& last = rez.back();
                    if (fullStartTimeMs + i->duration + filter.detailLevel.count() >= last.startTimeMs)
                    {
                        const auto endTimeMs = last.endTimeMs();
                        last.startTimeMs = qMin(last.startTimeMs, fullStartTimeMs);
                        last.durationMs = endTimeMs - last.startTimeMs;
                    }
                    else
                    {
                        if (rez.size() == filter.limit)
                            return;
                        rez.push_back(QnTimePeriod(fullStartTimeMs, i->duration));
                    }
                }
            }
            if (interruptionCallback && interruptionCallback())
                return;

            curData -= recordSize();
            --i;
        }
        totalSteps -= readed / recordSize();
        recordNumberToSeek = std::max(
            0, recordNumberToSeek - std::min(totalSteps, kRecordsPerIteration));

    }
}

QnTimePeriodList MetadataArchive::matchPeriodInternal(
    const Filter& filter,
    MatchExtraDataFunc matchExtraData,
    std::function<bool()> interruptionCallback)
{
    qint64 msStartTime = filter.startTime.count();
    qint64 msEndTime = filter.endTime.count();

    if (minTime() != (qint64)AV_NOPTS_VALUE)
        msStartTime = qMax(minTime(), msStartTime);
    msEndTime = qMin(msEndTime, m_maxMetadataTime.load());

    quint8* buffer = (quint8*)qMallocAligned(recordSize() * kRecordsPerIteration, 32);
    auto scopedGuard = nx::utils::makeScopeGuard(
        [buffer]()
    {
        qFreeAligned(buffer);
    });

    simd128i mask[kGridDataSizeBytes / sizeof(simd128i)];
    int maskStart, maskEnd;

    NX_ASSERT(!useSSE2() || ((std::ptrdiff_t)mask) % 16 == 0);
    QnMetaDataV1::createMask(filter.region, (char*)mask, &maskStart, &maskEnd);

    QnTimePeriodList rez;
    QFile metadataFile, indexFile;
    const bool descendingOrder = filter.sortOrder == Qt::SortOrder::DescendingOrder;

    while (msStartTime <= msEndTime)
    {
        qint64 minTime, maxTime;
        auto timePointMs = descendingOrder ? msEndTime : msStartTime;
        dateBounds(timePointMs, minTime, maxTime);

        QVector<IndexRecord> index;
        IndexHeader indexHeader;
        fillFileNames(timePointMs, &metadataFile, 0);
        NX_VERBOSE(this, lm("Matching motion periods for camera %1 for month %2")
            .args(m_uniqueId, metadataFile.fileName()));
        bool isFileExists = metadataFile.open(QFile::ReadOnly)
            && loadIndexFile(index, indexHeader, timePointMs);
        if (isFileExists)
        {
            QVector<IndexRecord>::iterator startItr = index.begin();
            QVector<IndexRecord>::iterator endItr = index.end();

            if (msStartTime > minTime)
            {
                const quint32 relativeStartTime = msStartTime - indexHeader.startTime;
                startItr = std::lower_bound(index.begin(), index.end(), relativeStartTime);
                while (startItr != index.end() && startItr != index.begin())
                {
                    const auto prevItr = startItr - 1;
                    if (qBetween(prevItr->start, relativeStartTime, prevItr->start + prevItr->duration))
                        startItr = prevItr;
                    else
                        break;
                }
            }
            if (msEndTime <= maxTime)
                endItr = std::upper_bound(startItr, index.end(), msEndTime - indexHeader.startTime);

            if (descendingOrder)
            {
                loadDataFromIndexDesc(
                    matchExtraData, interruptionCallback,
                    filter, metadataFile, indexHeader, index, startItr, endItr,
                    buffer, mask, maskStart, maskEnd, rez);
            }
            else
            {
                loadDataFromIndex(
                    matchExtraData, interruptionCallback,
                    filter, metadataFile, indexHeader, index, startItr, endItr,
                    buffer, mask, maskStart, maskEnd, rez);
            }

            if (filter.limit > 0 && rez.size() == filter.limit)
                break;
        }

        if (descendingOrder)
            msEndTime = minTime - 1;
        else
            msStartTime = maxTime + 1;

        if (interruptionCallback && interruptionCallback())
            break;
    }
    NX_VERBOSE(this, lm("Found %1 motion period(s) for camera %2 for range %3-%4")
        .args(rez.size(), m_uniqueId, filter.startTime, filter.endTime));
    return rez;
}

qint64 MetadataArchive::minTime() const
{
    return m_minMetadataTime;
}

qint64 MetadataArchive::maxTime() const
{
    return m_maxMetadataTime;
}

int MetadataArchive::getChannel() const
{
    return m_channel;
}

} // namespace nx::vms::server::metadata
