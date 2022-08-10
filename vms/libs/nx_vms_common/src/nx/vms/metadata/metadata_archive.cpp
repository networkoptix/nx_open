// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metadata_archive.h"

#include <QtCore/QDir>

#include <nx/utils/scope_guard.h>
#include <nx/utils/log/log_main.h>

#include "metadata_helper.h"
#include <utils/media/sse_helper.h>
#include <utils/math/math.h>

namespace nx::vms::metadata {

static const char kVersion = 2;
static const int kRecordsPerIteration = 1024;
static const int kAlignment = 16;
static const int kIndexRecordSize = sizeof(IndexRecord);
static const int kMetadataIndexHeaderSize = sizeof(IndexHeader);

Index::Index(MetadataArchive* owner): m_owner(owner)
{
    reset();
}

qint64 Index::dataFileSize() const
{
    return dataSize(0, records.size());
}

qint64 Index::dataOffset(int recordNumber) const
{
    return dataSize(0, recordNumber);
}

qint64 Index::dataSize(int from, int size) const
{
    qint64 words = 0;
    for (int i = from; i < from + size; ++i)
        words += records[i].extraWords();
    return header.baseRecordSize * size + words * header.wordSize;
}

qint64 Index::dataSize(const QVector<IndexRecord>::const_iterator& itr) const
{
    return header.baseRecordSize + itr->extraWords() * header.wordSize;
}

qint64 Index::indexFileSize() const
{
    return records.size() * kIndexRecordSize + kMetadataIndexHeaderSize;
}

void Index::reset()
{
    records.clear();
    header = IndexHeader();

    header.version = kVersion;
    header.baseRecordSize = m_owner->baseRecordSize();
    header.wordSize = m_owner->wordSize();
    header.intervalMs = m_owner->aggregationIntervalSeconds() * 1000;
}

void Index::truncate(qint64 dataFileSize)
{
    qint64 indexSize = 0;
    for (auto i = records.begin(); i < records.end(); ++i)
    {
        indexSize += dataSize(i);
        if (indexSize > dataFileSize)
        {
                NX_VERBOSE(this, "Metadata index is truncated from %1 to %2 records",
                    records.size(), std::distance(records.begin(), i));
            records.erase(i, records.end());
            break;
        }
    }
}

bool Index::load(const QDateTime& time)
{
    return load(time.toMSecsSinceEpoch());
}

bool Index::load(qint64 timestampMs)
{
    QFile indexFile;
    m_owner->fillFileNames(timestampMs, 0, &indexFile);
    if (!indexFile.open(QFile::ReadOnly))
        return false;
    return load(indexFile);
}

bool Index::load(QFile& indexFile)
{
    reset();
    indexFile.seek(0);
    int read = indexFile.read((char*)&header, kMetadataIndexHeaderSize);
    if (read != kMetadataIndexHeaderSize)
    {
        NX_VERBOSE(this, "Failed to load index file %1", indexFile.fileName());
        return false;
    }

    if (header.version < 2)
    {
        // For version < 2 these fields are empty.
        header.baseRecordSize = m_owner->baseRecordSize();
        header.wordSize = m_owner->wordSize();
    }

    records.resize((indexFile.size() - kMetadataIndexHeaderSize) / kIndexRecordSize);
    if (records.isEmpty())
        return true;
    const int sizeInBytes = kIndexRecordSize * records.size();
    read = indexFile.read((char*)records.data(), sizeInBytes);
    if (read != sizeInBytes)
        NX_VERBOSE(this, "Failed to load index file %1", indexFile.fileName());
    return read == sizeInBytes;
}

MetadataArchive::MetadataArchive(
    const QString& filePrefix,
    int baseRecordSize,
    int wordSize,
    int aggregationIntervalSeconds,
    const QString& dataDir,
    const QString& physicalId,
    int channel)
    :
    m_filePrefix(filePrefix),
    m_baseRecordSize(baseRecordSize),
    m_wordSize(wordSize),
    m_aggregationIntervalSeconds(aggregationIntervalSeconds),
    m_index(this),
    m_dataDir(dataDir),
    m_physicalId(physicalId),
    m_channel(channel)
{
    loadRecordedRange();
}

int MetadataArchive::baseRecordSize() const
{
    return m_baseRecordSize;
}

int MetadataArchive::wordSize() const
{
    return m_wordSize;
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
    QList<QDate> existsRecords = helper.recordedMonth(m_physicalId);
    if (existsRecords.isEmpty())
        return;

    Index index(this);
    index.load(existsRecords.first().startOfDay());
    if (!index.records.isEmpty())
        m_minMetadataTime = index.records.first().start + index.header.startTimeMs;
    if (existsRecords.size() > 1)
        index.load(existsRecords.last().startOfDay());
    if (!index.records.isEmpty())
        m_lastRecordedTime = m_maxMetadataTime = index.records.last().start + index.header.startTimeMs;
}

QString MetadataArchive::getFilePrefix(const QDate& datetime) const
{
    MetadataHelper helper(m_dataDir);
    return helper.getDirByDateTime(datetime, m_physicalId);
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

bool resizeFile(QFile* file, qint64 size)
{
    if (file->size() == size)
        return true;
    return file->resize(size);
}

bool MetadataArchive::openFiles(qint64 timestampMs)
{
    m_middleRecordNum = -1;
    m_index.reset();
    dateBounds(timestampMs, m_firstTime, m_lastDateForCurrentFile);

    fillFileNames(timestampMs, &m_detailedMetadataFile, &m_detailedIndexFile);

    auto dirName = getFilePrefix(QDateTime::fromMSecsSinceEpoch(timestampMs).date());
    QDir dir;
    dir.mkpath(dirName);

    if (!m_detailedMetadataFile.open(QFile::ReadWrite))
        return false;

    if (!m_detailedIndexFile.open(QFile::ReadWrite))
        return false;

    if (m_detailedIndexFile.size() == 0)
    {
        m_index.header.startTimeMs = m_firstTime;
        m_detailedIndexFile.write((const char*) &m_index.header, sizeof(IndexHeader));
    }
    else
    {
        m_index.load(m_detailedIndexFile);
        m_firstTime = m_index.header.startTimeMs;
        if (m_index.records.size() > 0)
            m_lastRecordedTime = m_maxMetadataTime = m_index.records.last().start + m_index.header.startTimeMs;
    }

    // Truncate biggest file. So, it is error checking.
    m_index.truncate(m_detailedMetadataFile.size());
    if (!resizeFile(&m_detailedIndexFile, m_index.indexFileSize()))
        return false;
    if (!resizeFile(&m_detailedMetadataFile, m_index.dataFileSize()))
        return false;

    m_detailedMetadataFile.seek(m_detailedMetadataFile.size());
    m_detailedIndexFile.seek(m_detailedIndexFile.size());
    return true;
}

bool MetadataArchive::saveToArchiveInternal(const QnConstAbstractCompressedMetadataPtr& data)
{
    NX_MUTEX_LOCKER lock(&m_writeMutex);

    const qint64 timestamp = data->timestamp / 1000;
    if (timestamp > m_lastDateForCurrentFile || timestamp < m_firstTime)
    {
        // go to new file
        if (!openFiles(timestamp))
            return false;
    }

    if (timestamp < m_lastRecordedTime)
    {
        // go to the file middle
        m_middleRecordNum = getSizeForTime(timestamp, true);
        m_detailedIndexFile.seek(m_middleRecordNum*kIndexRecordSize + kMetadataIndexHeaderSize);
        m_detailedMetadataFile.seek(m_index.dataOffset(m_middleRecordNum));
        m_minMetadataTime = qMin(m_minMetadataTime.load(), timestamp);
    }
    else if (m_middleRecordNum >= 0 && m_middleRecordNum < m_index.records.size())
    {
        qint64 timeInIndex = m_index.records[m_middleRecordNum].start + m_index.header.startTimeMs;
        if (timestamp > timeInIndex) {
            m_middleRecordNum = getSizeForTime(timestamp, false);
            m_detailedIndexFile.seek(m_middleRecordNum*kIndexRecordSize + kMetadataIndexHeaderSize);
            m_detailedMetadataFile.seek(m_index.dataOffset(m_middleRecordNum));
        }
    }

    /**
     * It is possible that timestamp < m_firstTime in case of time has been changed to the past
     * and timezone (or DST flag) has been changed at the same time.
     */
    quint32 relTime = quint32(qMax(0LL, timestamp - m_firstTime));
    quint32 duration = std::max(kMinimalDurationMs, quint32(data->m_duration / 1000));
    const int extraRecordSize = data->dataSize() - m_index.header.baseRecordSize;
    quint32 extraWords = 0;
    if (extraRecordSize > 0)
    {
        if (!NX_ASSERT(extraWords <= 255))
            return false;
        if (!NX_ASSERT(extraRecordSize % m_index.header.wordSize))
            return false;
        extraWords = extraRecordSize / m_index.header.wordSize;
    }

    if (m_detailedIndexFile.write((const char*)&relTime, 4) != 4)
    {
        NX_WARNING(this, "Failed to write index file for camera %1", m_physicalId);
    }
    quint32 durationAndWords = (duration & 0xffffff) + (extraWords << 24);
    if (m_detailedIndexFile.write((const char*)&durationAndWords, 4) != 4)
    {
        NX_WARNING(this, "Failed to write index file for camera %1", m_physicalId);
    }

    if (m_detailedMetadataFile.write(data->data(), data->dataSize())
        != static_cast<qint64>(data->dataSize()))
    {
        NX_WARNING(this, "Failed to write index file for camera %1", m_physicalId);
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

void MetadataArchive::dateBounds(qint64 datetimeMs, qint64& minDate, qint64& maxDate) const
{
    QDateTime datetime = QDateTime::fromMSecsSinceEpoch(datetimeMs);
    QDateTime monthStart = datetime.date().addDays(1 - datetime.date().day()).startOfDay();
    minDate = monthStart.toMSecsSinceEpoch();

    QDateTime nextMonth(monthStart.addMonths(1));
    maxDate = nextMonth.toMSecsSinceEpoch() - 1;
}

int MetadataArchive::getSizeForTime(qint64 timeMs, bool reloadIndex)
{
    if (reloadIndex)
        m_index.load(m_detailedIndexFile);
    auto indexIterator = std::lower_bound(
        m_index.records.begin(), m_index.records.end(), timeMs - m_index.header.startTimeMs);
    return indexIterator - m_index.records.begin();
}

void MetadataArchive::loadDataFromIndex(
    RecordMatcher* recordMatcher,
    std::function<bool()> interruptionCallback,
    QFile& metadataFile,
    const Index& index,
    QVector<IndexRecord>::iterator startItr,
    const QVector<IndexRecord>::iterator endItr,
    QnTimePeriodList& rez) const
{
    QnByteArray buffer(kAlignment, 0, 0);

    int totalSteps = endItr - startItr;

    int currentRecordNumber = startItr - index.records.begin();
    qint64 dataOffset = index.dataOffset(currentRecordNumber);
    metadataFile.seek(dataOffset);

    // math file (one month)
    QVector<IndexRecord>::const_iterator i = startItr;
    while (totalSteps > 0)
    {
        const int recordsToRead = qMin(totalSteps, kRecordsPerIteration);
        const int dataSize = index.dataSize(currentRecordNumber, recordsToRead);
        buffer.reserve(dataSize);
        const int read = metadataFile.read(buffer.data(), dataSize);
        if (read <= 0)
            break;
        const quint8* dataEnd = (quint8*) buffer.data() + read;
        quint8* curData = (quint8*) buffer.data();
        while (i < endItr && curData < dataEnd)
        {
            const qint64 fullStartTimeMs = i->start + index.header.startTimeMs;
            const int dataSize = index.dataSize(i);
            if (curData + dataSize > dataEnd)
            {
                NX_DEBUG(this, "Unexpected metadata file size");
                break;
            }
            if (recordMatcher->matchRecord(fullStartTimeMs, curData, dataSize))
            {
                if (rez.empty())
                {
                    rez.push_back(QnTimePeriod(fullStartTimeMs, i->duration()));
                }
                else
                {
                    QnTimePeriod& last = rez.back();
                    if (fullStartTimeMs <= last.startTimeMs + last.durationMs
                        + recordMatcher->filter()->detailLevel.count())
                    {
                        last.durationMs = qMax(last.durationMs, i->duration() + fullStartTimeMs
                            - last.startTimeMs);
                    }
                    else
                    {
                        if (rez.size() == recordMatcher->filter()->limit)
                            return;
                        rez.push_back(QnTimePeriod(fullStartTimeMs, i->duration()));
                    }
                }
            }
            if (interruptionCallback && interruptionCallback())
                return;

            curData += index.dataSize(i);
            ++i;
            --totalSteps;
        }
        currentRecordNumber += recordsToRead;
    }
}

void MetadataArchive::loadDataFromIndexDesc(
    RecordMatcher* recordMatcher,
    std::function<bool()> interruptionCallback,
    QFile& metadataFile,
    const Index& index,
    QVector<IndexRecord>::iterator startItr,
    const QVector<IndexRecord>::iterator endItr,
    QnTimePeriodList& rez) const
{
    QnByteArray buffer(kAlignment, 0, 0);

    int totalSteps = endItr - startItr;
    int recordNumberToSeek = qMax<int>(0LL, (endItr - std::min(kRecordsPerIteration, totalSteps))
        - index.records.begin());

    // math file (one month)
    QVector<IndexRecord>::const_iterator i = endItr - 1;
    while (totalSteps > 0)
    {
        metadataFile.seek(index.dataOffset(recordNumberToSeek));
        const int recordsToRead = qMin(totalSteps, kRecordsPerIteration);
        int dataSize = index.dataSize(recordNumberToSeek, recordsToRead);
        buffer.reserve(dataSize);

        const int read = metadataFile.read(buffer.data(), dataSize);
        if (read <= 0)
            break;

        quint8* dataEnd = (quint8*) buffer.data() + read;
        quint8* curData = dataEnd - index.dataSize(i);
        while (i >= startItr && curData >= (quint8*) buffer.data())
        {
            qint64 fullStartTimeMs = i->start + index.header.startTimeMs;
            if (recordMatcher->matchRecord(fullStartTimeMs, curData, index.dataSize(i)))
            {
                if (rez.empty())
                {
                    rez.push_back(QnTimePeriod(fullStartTimeMs, i->duration()));
                }
                else
                {
                    QnTimePeriod& last = rez.back();
                    const auto detailLevel = recordMatcher->filter()->detailLevel.count();
                    if (fullStartTimeMs + i->duration() + detailLevel >= last.startTimeMs)
                    {
                        const auto endTimeMs = last.endTimeMs();
                        last.startTimeMs = qMin(last.startTimeMs, fullStartTimeMs);
                        last.durationMs = endTimeMs - last.startTimeMs;
                    }
                    else
                    {
                        if (rez.size() == recordMatcher->filter()->limit)
                            return;
                        rez.push_back(QnTimePeriod(fullStartTimeMs, i->duration()));
                    }
                }
            }
            if (interruptionCallback && interruptionCallback())
                return;

            --i;
            if (i >= startItr)
                curData -= index.dataSize(i);
            --totalSteps;
        }
        recordNumberToSeek = std::max(
            0, recordNumberToSeek - std::min(totalSteps, kRecordsPerIteration));

    }
}

QnTimePeriodList MetadataArchive::matchPeriodInternal(
    RecordMatcher* recordMatcher,
    std::function<bool()> interruptionCallback)
{
    qint64 msStartTime = recordMatcher->filter()->startTime.count();
    qint64 msEndTime = recordMatcher->filter()->endTime.count();

    if (minTime() != (qint64)AV_NOPTS_VALUE)
        msStartTime = qMax(minTime(), msStartTime);
    msEndTime = qMin(msEndTime, m_maxMetadataTime.load());

    QnTimePeriodList rez;
    const bool descendingOrder = recordMatcher->filter()->sortOrder == Qt::SortOrder::DescendingOrder;

    while (msStartTime <= msEndTime)
    {
        qint64 minTime, maxTime;
        const auto timePointMs = descendingOrder ? msEndTime : msStartTime;
        dateBounds(timePointMs, minTime, maxTime);

        QFile metadataFile;
        Index index(this);

        fillFileNames(timePointMs, &metadataFile, nullptr);
        NX_VERBOSE(this, nx::format("Matching metadata periods for camera %1 for month %2")
            .args(m_physicalId, metadataFile.fileName()));
        const bool isFileExists = metadataFile.open(QFile::ReadOnly) && index.load(timePointMs);
        if (isFileExists)
        {
            index.truncate(metadataFile.size());

            QVector<IndexRecord>::iterator startItr = index.records.begin();
            QVector<IndexRecord>::iterator endItr = index.records.end();

            if (msStartTime > minTime)
            {
                const quint32 relativeStartTime = msStartTime - index.header.startTimeMs;
                startItr = std::lower_bound(index.records.begin(), index.records.end(), relativeStartTime);
                while (startItr != index.records.end() && startItr != index.records.begin())
                {
                    const auto prevItr = startItr - 1;
                    if (qBetween(prevItr->start, relativeStartTime, prevItr->start + prevItr->duration()))
                        startItr = prevItr;
                    else
                        break;
                }
            }
            if (msEndTime <= maxTime)
                endItr = std::upper_bound(startItr, index.records.end(), msEndTime - index.header.startTimeMs);

            if (descendingOrder)
            {
                loadDataFromIndexDesc(
                    recordMatcher, interruptionCallback,
                    metadataFile, index, startItr, endItr,
                    rez);
            }
            else
            {
                loadDataFromIndex(
                    recordMatcher, interruptionCallback,
                    metadataFile, index, startItr, endItr,
                    rez);
            }

            if (recordMatcher->filter()->limit > 0 && rez.size() == recordMatcher->filter()->limit)
                break;
        }

        if (descendingOrder)
            msEndTime = minTime - 1;
        else
            msStartTime = maxTime + 1;

        if (interruptionCallback && interruptionCallback())
            break;
    }
    NX_VERBOSE(this, nx::format("Found %1 metadata period(s) for camera %2 for range %3-%4")
        .args(rez.size(), m_physicalId, recordMatcher->filter()->startTime, recordMatcher->filter()->endTime));
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

QString MetadataArchive::physicalId() const
{
    return m_physicalId;
}

} // namespace nx::vms::metadata
