// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metadata_archive.h"

#include <QtCore/QDir>

#include <nx/media/sse_helper.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/math/math.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/algorithm.h>
#include <utils/common/synctime.h>

#include "metadata_helper.h"

namespace nx::vms::metadata {

static const char kVersion = 4;
static const int kReadBufferSize = 1024 * 256;
static const int kAlignment = 16;
static const int kIndexRecordSize = sizeof(IndexRecord);
static const int kMetadataIndexHeaderSize = sizeof(IndexHeader);

bool resizeFile(QFile* file, qint64 size)
{
    if (file->size() == size)
        return true;
    return file->resize(size);
}

inline bool checkPeriod(const Filter* filter, qint64 startTimeMs, qint32 durationMs)
{
    return startTimeMs <= filter->endTime.count()
        && startTimeMs + durationMs >= filter->startTime.count();
}

Index::Index(MetadataArchive* owner): m_owner(owner)
{
    reset();
}

qint64 Index::indexFileSize() const
{
    return records.size() * kIndexRecordSize + kMetadataIndexHeaderSize;
}

qint64 Index::mediaFileSize(bool noGeometryMode) const
{
    return dataOffset(records.size(), noGeometryMode);
}

qint64 Index::dataOffset(int indexRecordNumber, bool noGeometryMode) const
{
    qint64 result = 0;
    const int baseRecordSize = noGeometryMode ? header.noGeometryRecordSize() : header.recordSize;
    for (int i = 0; i < indexRecordNumber; ++i)
    {
        const auto count = records[i].recordCount();
        int recordSize = (baseRecordSize + records[i].extraWords * header.wordSize) * count;
        result += recordSize;
    }
    return result;
}

qint64 Index::mediaSize(int from, int suggestedbufferSize, bool noGeometryMode) const
{
    const int baseRecordSize = noGeometryMode ? header.noGeometryRecordSize() : header.recordSize;
    qint64 result = 0;
    for (int i = from; i < records.size(); ++i)
    {
        const int count = records[i].recordCount();
        int recordSize = (baseRecordSize + records[i].extraWords * header.wordSize) * count;
        suggestedbufferSize -= recordSize;
        result += recordSize;
        if (suggestedbufferSize <= 0)
            break;
    }
    return result;
}

qint64 Index::mediaSizeDesc(int from, int suggestedbufferSize, bool noGeometryMode) const
{
    const int baseRecordSize = noGeometryMode ? header.noGeometryRecordSize() : header.recordSize;
    qint64 result = 0;
    for (int i = from; i >= 0; --i)
    {
        const int count = records[i].recordCount();
        int recordSize = (baseRecordSize + records[i].extraWords * header.wordSize) * count;
        suggestedbufferSize -= recordSize;
        result += recordSize;
        if (suggestedbufferSize <= 0)
            break;
    }
    return result;
}

qint64 Index::extraRecordSize(int index) const
{
    return records[index].extraWords * header.wordSize;
}

void Index::reset()
{
    records.clear();
    header = IndexHeader();

    header.version = kVersion;
    header.recordSize = m_owner->baseRecordSize();
    header.intervalMs = m_owner->aggregationIntervalSeconds() * 1000;
    header.wordSize = m_owner->wordSize();

    memset(header.dummy, 0, sizeof(header.dummy));
}

bool Index::updateTail(QFile* indexFile)
{
    if (!resizeFile(indexFile, indexFileSize()))
    {
        NX_WARNING(this, "Failed to resize file %1 to size %2",
            indexFile->fileName(), indexFileSize());
        return false;
    }

    // rewrite last record if exists
    if (!records.isEmpty())
        indexFile->seek(kMetadataIndexHeaderSize + (records.size() - 1) * kIndexRecordSize);
    return indexFile->write((const char*)&records.last(), kIndexRecordSize) == kIndexRecordSize;
}

bool Index::truncateToBytes(qint64 mediaSize, bool noGeometryMode)
{
    int64_t processedBytes = 0;
    bool isModified = false;
    const int baseRecordSize = noGeometryMode
        ? header.noGeometryRecordSize() : header.recordSize;

    for (int i = 0; i < records.size(); ++i)
    {
        const int indexRecords = records[i].recordCount();
        const int fullRecordSize = baseRecordSize + records[i].extraWords * header.wordSize;
        if (processedBytes + indexRecords * fullRecordSize > mediaSize)
        {
            isModified = true;
            int newIndexRecords = (mediaSize - processedBytes) / fullRecordSize;
            if (newIndexRecords > 0)
            {
                NX_DEBUG(this, "Truncating index record %1 from %2 to %3",
                    i, indexRecords, newIndexRecords);
                records[i].setRecordCount(newIndexRecords);
                processedBytes += newIndexRecords * fullRecordSize;
                ++i;
                if (i == records.size())
                    break;
            }
            NX_VERBOSE(this, "Metadata index is truncated from %1 to %2 records",
                records.size(), i);
            records.erase(records.begin() + i, records.end());
            break;
        }
        processedBytes += indexRecords * fullRecordSize;
    }
    return isModified;
}

bool Index::load(const QDateTime& time)
{
    return load(time.toMSecsSinceEpoch());
}

bool Index::load(qint64 timestampMs)
{
    QFile indexFile;
    m_owner->fillFileNames(timestampMs, &indexFile);
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
        header.recordSize = m_owner->baseRecordSize();
    }
    if (header.version < 4)
        header.wordSize = 0;

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
    m_recordSize(baseRecordSize),
    m_wordSize(wordSize),
    m_aggregationIntervalSeconds(aggregationIntervalSeconds),
    m_index(this),
    m_dataDir(dataDir),
    m_physicalId(physicalId),
    m_channel(channel)
{
}

int MetadataArchive::baseRecordSize() const
{
    return m_recordSize;
}

int MetadataArchive::wordSize() const
{
    return m_wordSize;
}

int MetadataArchive::aggregationIntervalSeconds() const
{
    return m_aggregationIntervalSeconds;
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

void MetadataArchive::fillFileNames(qint64 datetimeMs, QFile* indexFile) const
{
    fillFileNames(datetimeMs, nullptr, false, indexFile);
}

void MetadataArchive::fillFileNames(qint64 datetimeMs, QFile* metadataFile, bool noGeometry, QFile* indexFile) const
{
    QDateTime datetime = QDateTime::fromMSecsSinceEpoch(datetimeMs);
    QString fileName = getFilePrefix(datetime.date());
    if (metadataFile)
    {
        metadataFile->close();
        const QString fileType = noGeometry ? "nogeometry" : "detailed";
        metadataFile->setFileName(fileName + QString("%1_%2_data").arg(m_filePrefix).arg(fileType) + getChannelPrefix() + ".bin");
    }
    if (indexFile)
    {
        indexFile->close();
        indexFile->setFileName(fileName + QString("%1_detailed_index").arg(m_filePrefix) + getChannelPrefix() + ".bin");
    }
}

bool MetadataArchive::openFiles(qint64 timestampMs)
{
    m_index.reset();
    dateBounds(timestampMs, m_firstTime, m_lastDateForCurrentFile);

    fillFileNames(timestampMs, &m_detailedMetadataFile, /*noGeometry*/ false, &m_detailedIndexFile);

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
            m_lastRecordedTime = m_index.records.last().start + m_index.header.startTimeMs;
    }

    if (m_index.header.noGeometryModeSupported() && m_index.header.noGeometryRecordSize() > 0)
    {
        auto name = m_detailedMetadataFile.fileName();
        name = name.replace("detailed", "nogeometry");
        m_noGeometryMetadataFile = std::make_unique<QFile>(name);
        m_noGeometryMetadataFile->open(QFile::ReadWrite);
    }

    bool isUpdated = m_index.truncateToBytes(m_detailedMetadataFile.size(), /*noGeometryMode*/ false);
    if (m_noGeometryMetadataFile)
    {
        isUpdated |= m_index.truncateToBytes(m_noGeometryMetadataFile->size(), /*noGeometryMode*/ true);
    }
    if (isUpdated)
    {
        if (!m_index.updateTail(&m_detailedIndexFile))
            return false;
    }

    qint64 newMetadataFileSize = m_index.mediaFileSize(/*noGeometryMode*/ false);
    if (!resizeFile(&m_detailedMetadataFile, newMetadataFileSize))
    {
        NX_WARNING(this, "Failed to resize file %1 to size %2",
            m_detailedMetadataFile.fileName(), newMetadataFileSize);
        return false;
    }
    if (m_noGeometryMetadataFile)
    {
        qint64 newNoGeometryFileSize = m_index.mediaFileSize(/*noGeometryMode*/ true);
        if (!resizeFile(m_noGeometryMetadataFile.get(), newNoGeometryFileSize))
        {
            NX_WARNING(this, "Failed to resize file %1 to size %2",
                m_noGeometryMetadataFile->fileName(), newNoGeometryFileSize);
            return false;
        }
        m_noGeometryMetadataFile->seek(m_noGeometryMetadataFile->size());
    }

    m_detailedMetadataFile.seek(m_detailedMetadataFile.size());
    m_detailedIndexFile.seek(m_detailedIndexFile.size());

    return true;
}

bool MetadataArchive::supportVariousRecordSize(std::chrono::milliseconds timestamp)
{
    if (timestamp.count() > m_lastDateForCurrentFile || timestamp.count() < m_firstTime)
    {
        if (!openFiles(timestamp.count()))
            return false;
    }

    return m_index.header.variousRecordSizeSupported();
}

bool MetadataArchive::saveToArchiveInternal(
    const QnConstAbstractCompressedMetadataPtr& data, int extraWords)
{
    NX_MUTEX_LOCKER lock(&m_writeMutex);

    const int  extraDataPerRecord = extraWords * m_index.header.wordSize;
    const int fullRecordSize = m_index.header.recordSize + extraDataPerRecord;
    const int recordCount = data->dataSize() / fullRecordSize;

    const qint64 timestamp = data->timestamp / 1000;
    if (timestamp > m_lastDateForCurrentFile || timestamp < m_firstTime)
    {
        // go to new file
        if (!openFiles(timestamp))
            return false;
    }

    if (timestamp < m_lastRecordedTime)
    {
        NX_WARNING(this,
            "Timestamp discontinuity %1 detected for metadata binary index, "
            "previous time=%2, camera=%3", timestamp, m_lastRecordedTime, m_physicalId);
        if (!(m_index.header.flags & IndexHeader::Flags::hasDiscontinue))
        {
            m_index.header.flags |= IndexHeader::Flags::hasDiscontinue;
            m_detailedIndexFile.seek(0);
            m_detailedIndexFile.write((const char*)&m_index.header, sizeof(IndexHeader));
            m_detailedIndexFile.seek(m_detailedIndexFile.size());
        }
    }

    /**
     * It is possible that timestamp < m_firstTime in case of time has been changed to the past
     * and timezone (or DST flag) has been changed at the same time.
     */
    IndexRecord indexRecord;
    indexRecord.start = quint32(qMax(0LL, timestamp - m_firstTime));

    quint32 duration = std::max(kMinimalDurationMs, quint32(data->m_duration / 1000));
    NX_ASSERT(data->dataSize() % (m_index.header.recordSize + extraDataPerRecord) == 0);
    NX_ASSERT(duration < 0x10000);

    if (m_noGeometryMetadataFile)
    {
        const char* dataEx = data->data() + kGeometrySize;
        const int recordExSize = fullRecordSize - kGeometrySize;
        for (int i = 0; i < recordCount; ++i)
        {
            if (m_noGeometryMetadataFile->write(dataEx, recordExSize) != recordExSize)
                NX_WARNING(this, "Failed to write nogeometry file for camera %1", m_physicalId);
            dataEx += fullRecordSize;
        }
    }

    indexRecord.duration = duration & 0xffff;
    indexRecord.extraWords = extraWords;
    indexRecord.recCount = recordCount - 1;

    if (m_detailedIndexFile.write((const char* )&indexRecord, sizeof(IndexRecord)) != sizeof(IndexRecord))
    {
        NX_WARNING(this, "Failed to write index file for camera %1", m_physicalId);
    }

    if (m_detailedMetadataFile.write(data->data(), data->dataSize())
        != static_cast<qint64>(data->dataSize()))
    {
        NX_WARNING(this, "Failed to write index file for camera %1", m_physicalId);
    }

    m_detailedMetadataFile.flush();
    if (m_noGeometryMetadataFile)
        m_noGeometryMetadataFile->flush();
    m_detailedIndexFile.flush();

    m_lastRecordedTime = timestamp;
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

bool addToResultAsc(
    const Filter* filter,
    qint64 fullStartTimeMs,
    qint32 durationMs,
    QnTimePeriodList& rez)
{
    if (rez.empty())
    {
        rez.push_back(QnTimePeriod(fullStartTimeMs, durationMs));
        return true;
    }

    QnTimePeriod& last = rez.back();
    if (!rez.empty() && fullStartTimeMs <= last.startTimeMs
        + last.durationMs + filter->detailLevel.count())
    {
        last.durationMs = qMax(last.durationMs, durationMs + fullStartTimeMs - last.startTimeMs);
    }
    else
    {
        if (rez.size() == filter->limit)
            return false;
        rez.push_back(QnTimePeriod(fullStartTimeMs, durationMs));
    }
    return true;
}

bool addToResultAscUnordered(
    const Filter* filter,
    qint64 fullStartTimeMs,
    qint32 durationMs,
    QnTimePeriodList& rez)
{
    if (rez.empty())
    {
        rez.push_back(QnTimePeriod(fullStartTimeMs, durationMs));
        return true;
    }

    QnTimePeriod& last = rez.back();
    if (fullStartTimeMs >= last.startTimeMs
        && fullStartTimeMs <= last.startTimeMs + last.durationMs + filter->detailLevel.count())
    {
        last.durationMs = qMax(last.durationMs, durationMs + fullStartTimeMs - last.startTimeMs);
    }
    else
    {
        rez.push_back(QnTimePeriod(fullStartTimeMs, durationMs));
    }
    return true;
}

bool addToResultDesc(
    const Filter* filter,
    qint64 fullStartTimeMs,
    qint32 durationMs,
    QnTimePeriodList& rez)
{
    if (rez.empty())
    {
        rez.push_back(QnTimePeriod(fullStartTimeMs, durationMs));
        return true;
    }

    QnTimePeriod& last = rez.back();
    const auto detailLevel = filter->detailLevel.count();
    if (fullStartTimeMs + durationMs + detailLevel >= last.startTimeMs)
    {
        const auto endTimeMs = last.endTimeMs();
        last.startTimeMs = qMin(last.startTimeMs, fullStartTimeMs);
        last.durationMs = endTimeMs - last.startTimeMs;
    }
    else
    {
        if (rez.size() == filter->limit)
            return false;
        rez.push_back(QnTimePeriod(fullStartTimeMs, durationMs));
    }
    return true;
}

bool addToResultDescUnordered(
    const Filter* filter,
    qint64 fullStartTimeMs,
    qint32 durationMs,
    QnTimePeriodList& rez)
{
    if (rez.empty())
    {
        rez.push_back(QnTimePeriod(fullStartTimeMs, durationMs));
        return true;
    }

    QnTimePeriod& last = rez.back();
    const auto detailLevel = filter->detailLevel.count();
    if (fullStartTimeMs <= last.startTimeMs
        && fullStartTimeMs + durationMs + detailLevel >= last.startTimeMs)
    {
        const auto endTimeMs = last.endTimeMs();
        last.startTimeMs = qMin(last.startTimeMs, fullStartTimeMs);
        last.durationMs = endTimeMs - last.startTimeMs;
    }
    else
    {
        rez.push_back(QnTimePeriod(fullStartTimeMs, durationMs));
    }
    return true;
}

void MetadataArchive::loadDataFromIndex(
    AddRecordFunc addRecordFunc,
    RecordMatcher* recordMatcher,
    std::function<bool()> interruptionCallback,
    QFile& metadataFile,
    const Index& index,
    QVector<IndexRecord>::iterator startItr,
    const QVector<IndexRecord>::iterator endItr,
    QnTimePeriodList& rez) const
{
    nx::utils::ByteArray buffer(kAlignment, 0, 0);

    int mediaRecordsLeft = 0;
    for (auto i = startItr; i < endItr; ++i)
        mediaRecordsLeft += i->recordCount();
    if (mediaRecordsLeft == 0)
        return;

    const int baseRecordSize = recordMatcher->isNoGeometryMode()
        ? index.header.noGeometryRecordSize() : index.header.recordSize;

    int indexRecordNumber = startItr - index.records.begin();
    qint64 dataOffset = index.dataOffset(indexRecordNumber, recordMatcher->isNoGeometryMode());
    metadataFile.seek(dataOffset);

    QVector<IndexRecord>::const_iterator i = startItr;
    int mediaRecordsPerIndexRecord = i->recordCount();
    while (mediaRecordsLeft > 0)
    {
        const int bufferSize = index.mediaSize(indexRecordNumber, kReadBufferSize, recordMatcher->isNoGeometryMode());

        buffer.reserve(bufferSize);
        const int read = metadataFile.read(buffer.data(), bufferSize);
        if (read <= 0)
            break;
        const quint8* dataEnd = (quint8*) buffer.data() + read;
        quint8* curData = (quint8*) buffer.data();
        const auto filter = recordMatcher->filter();
        while (i < endItr && curData < dataEnd)
        {
            const qint64 fullStartTimeMs = i->start + index.header.startTimeMs;
            int recordSize = baseRecordSize + index.extraRecordSize(indexRecordNumber);
            if (checkPeriod(filter, fullStartTimeMs, i->duration))
            {
                if (curData + recordSize > dataEnd)
                {
                    NX_DEBUG(this, "Unexpected metadata file size");
                    break;
                }
                if (recordMatcher->matchRecord(fullStartTimeMs, curData, recordSize))
                {
                if (!addRecordFunc(recordMatcher->filter(), fullStartTimeMs, i->duration, rez))
                        return;
                }
                if (interruptionCallback && interruptionCallback())
                    return;
            }

            curData += recordSize;
            --mediaRecordsLeft;
            if (--mediaRecordsPerIndexRecord == 0)
            {
                if (++i < endItr)
                    mediaRecordsPerIndexRecord = i->recordCount();
                ++indexRecordNumber;
            }
        }
    }
}

void MetadataArchive::loadDataFromIndex(
    AddRecordFunc addRecordFunc,
    const Filter* filter,
    std::function<bool()> interruptionCallback,
    const Index& index,
    QVector<IndexRecord>::iterator startItr,
    const QVector<IndexRecord>::iterator endItr,
    QnTimePeriodList& rez) const
{
    for(auto i = startItr; i < endItr;  ++i)
    {
        const qint64 fullStartTimeMs = i->start + index.header.startTimeMs;
        if (!checkPeriod(filter, fullStartTimeMs, i->duration))
            continue;
        if (!addRecordFunc(filter, fullStartTimeMs, i->duration, rez))
            return;
        if (interruptionCallback && interruptionCallback())
            return;
    }
}

void MetadataArchive::loadDataFromIndexDesc(
    AddRecordFunc addRecordFunc,
    RecordMatcher* recordMatcher,
    std::function<bool()> interruptionCallback,
    QFile& metadataFile,
    const Index& index,
    QVector<IndexRecord>::iterator startItr,
    const QVector<IndexRecord>::iterator endItr,
    QnTimePeriodList& rez) const
{
    nx::utils::ByteArray buffer(kAlignment, 0, 0);

    const int baseRecordSize = recordMatcher->isNoGeometryMode()
        ? index.header.noGeometryRecordSize() : index.header.recordSize;

    int mediaRecordsLeft = 0;
    for (auto i = startItr; i < endItr; ++i)
        mediaRecordsLeft += i->recordCount();
    if (mediaRecordsLeft == 0)
        return;

    // math file (one month)
    QVector<IndexRecord>::const_iterator i = endItr - 1;
    const QVector<IndexRecord>::const_iterator itBegin = index.records.begin();
    int64_t mediaFileOffset = index.dataOffset(endItr - itBegin, recordMatcher->isNoGeometryMode());
    while (mediaRecordsLeft > 0)
    {
        const int mediaDataSize = index.mediaSizeDesc(i - itBegin,
            kReadBufferSize, recordMatcher->isNoGeometryMode());

        mediaFileOffset -= mediaDataSize;
        metadataFile.seek(mediaFileOffset);

        buffer.reserve(mediaDataSize);
        const int read = metadataFile.read(buffer.data(), mediaDataSize);
        if (read <= 0)
            break;

        int mediaRecordsPerIndexRecord = i->recordCount();
        int fullRecordSize = baseRecordSize + index.extraRecordSize(i - itBegin);
        quint8* curData = (quint8*)buffer.data() + read;
        const auto filter = recordMatcher->filter();
        while (i >= startItr && curData > (quint8*) buffer.data())
        {
            curData -= fullRecordSize;
            qint64 fullStartTimeMs = i->start + index.header.startTimeMs;
            if (checkPeriod(filter, fullStartTimeMs, i->duration))
            {
                if (recordMatcher->matchRecord(fullStartTimeMs, curData, fullRecordSize))
                {
                    if (!addRecordFunc(filter, fullStartTimeMs, i->duration, rez))
                        return;
                }
                if (interruptionCallback && interruptionCallback())
                    return;
            }

            --mediaRecordsLeft;
            if (--mediaRecordsPerIndexRecord == 0)
            {
                if (--i >= startItr)
                {
                    mediaRecordsPerIndexRecord = i->recordCount();
                    fullRecordSize = baseRecordSize + index.extraRecordSize(i - itBegin);
                }
            }
        }
    }
}

void MetadataArchive::loadDataFromIndexDesc(
    AddRecordFunc addRecordFunc,
    const Filter* filter,
    std::function<bool()> interruptionCallback,
    const Index& index,
    QVector<IndexRecord>::iterator startItr,
    const QVector<IndexRecord>::iterator endItr,
    QnTimePeriodList& rez) const
{
    for (auto i = endItr - 1; i >= startItr; --i)
    {
        const qint64 fullStartTimeMs = i->start + index.header.startTimeMs;
        if (!checkPeriod(filter, fullStartTimeMs, i->duration))
            continue;

        if (!addRecordFunc(filter, fullStartTimeMs, i->duration, rez))
            return;
        if (interruptionCallback && interruptionCallback())
            return;
    }
}

QnTimePeriodList MetadataArchive::matchPeriodInternal(
    RecordMatcher* recordMatcher,
    std::function<bool()> interruptionCallback,
    FixDiscontinue fixDiscontinue)
{
    MetadataHelper helper(m_dataDir);
    QList<QDate> existingRecords = helper.recordedMonth(m_physicalId);
    if (existingRecords.isEmpty())
        return QnTimePeriodList();

    qint64 msStartTime = std::max(
        (qint64) recordMatcher->filter()->startTime.count(),
        existingRecords.first().startOfDay().toMSecsSinceEpoch());
    qint64 msEndTime = std::min(
        (qint64) recordMatcher->filter()->endTime.count(),
        existingRecords.last().startOfDay().addMonths(1).toMSecsSinceEpoch() - 1);

    QnTimePeriodList rez;
    const bool descendingOrder = recordMatcher->filter()->sortOrder == Qt::SortOrder::DescendingOrder;
    bool needToFixDiscontinuity = false;
    while (msStartTime <= msEndTime)
    {
        qint64 minTime, maxTime;
        const auto timePointMs = descendingOrder ? msEndTime : msStartTime;
        dateBounds(timePointMs, minTime, maxTime);

        QFile metadataFile;
        Index index(this);

        if (index.load(timePointMs))
        {
            recordMatcher->onFileOpened(timePointMs, index.header);
            if (recordMatcher->isNoData())
                return rez;
            const bool hasDiscontinue = index.header.flags & IndexHeader::Flags::hasDiscontinue;
            needToFixDiscontinuity |= hasDiscontinue;

            const bool noGeometryMode = index.header.noGeometryModeSupported()
                && index.header.noGeometryRecordSize() > 0
                && recordMatcher->isWholeFrame();
            recordMatcher->setNoGeometryMode(noGeometryMode);
            fillFileNames(timePointMs, &metadataFile, noGeometryMode, nullptr);
            NX_VERBOSE(this, nx::format("Matching metadata periods for camera %1 for month %2")
                .args(m_physicalId, metadataFile.fileName()));

            if (recordMatcher->isEmpty() || metadataFile.open(QFile::ReadOnly))
            {
                const int recordSize = noGeometryMode
                    ? index.header.noGeometryRecordSize() : index.header.recordSize;
                if (metadataFile.isOpen())
                    index.truncateToBytes(metadataFile.size(), noGeometryMode);

                QVector<IndexRecord>::iterator startItr = index.records.begin();
                QVector<IndexRecord>::iterator endItr = index.records.end();

                if (!hasDiscontinue)
                {
                    if (msStartTime > minTime)
                    {
                        const quint32 relativeStartTime = msStartTime - index.header.startTimeMs;
                        startItr = std::lower_bound(index.records.begin(), index.records.end(), relativeStartTime);
                        while (startItr != index.records.end() && startItr != index.records.begin())
                        {
                            const auto prevItr = startItr - 1;
                            if (qBetween(prevItr->start, relativeStartTime, prevItr->start + prevItr->duration))
                                startItr = prevItr;
                            else
                                break;
                        }
                    }
                    if (msEndTime <= maxTime)
                        endItr = std::upper_bound(startItr, index.records.end(), msEndTime - index.header.startTimeMs);
                }

                // Turn off limit checking for discontinue mode. It is needed to apply limit after sorting data.
                const auto breakCallback = hasDiscontinue ? nullptr : interruptionCallback;

                // Match one file (one month of data or less).
                if (recordMatcher->isEmpty())
                {
                    if (descendingOrder)
                    {
                        loadDataFromIndexDesc(
                            hasDiscontinue ? addToResultDescUnordered : addToResultDesc,
                            recordMatcher->filter(),
                            breakCallback, index, startItr, endItr, rez);
                    }
                    else
                    {
                        loadDataFromIndex(
                            hasDiscontinue ? addToResultAscUnordered : addToResultAsc,
                            recordMatcher->filter(),
                            breakCallback, index, startItr, endItr, rez);
                    }
                }
                else
                {
                    if (descendingOrder)
                    {
                        loadDataFromIndexDesc(
                            hasDiscontinue ? addToResultDescUnordered : addToResultDesc,
                            recordMatcher, breakCallback,
                            metadataFile, index, startItr, endItr,
                            rez);
                    }
                    else
                    {
                        loadDataFromIndex(
                            hasDiscontinue ? addToResultAscUnordered : addToResultAsc,
                            recordMatcher, breakCallback,
                            metadataFile, index, startItr, endItr,
                            rez);
                    }
                }

                if (!hasDiscontinue
                    && recordMatcher->filter()->limit > 0
                    && rez.size() >= recordMatcher->filter()->limit)
                {
                    break;
                }
            }
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

    if (needToFixDiscontinuity)
        fixDiscontinue(&rez, descendingOrder, recordMatcher->filter());
    return rez;
}

void MetadataArchive::onDiscontinueInMatchedResult(
    QnTimePeriodList* result, bool descendingOrder, const Filter* filter)
{
    sortData(result, descendingOrder);

    QnTimePeriodList fixedResult;
    auto addRecord = descendingOrder ? addToResultDesc : addToResultAsc;
    for (const auto& period: *result)
    {
        if (!addRecord(filter, period.startTimeMs, period.durationMs, fixedResult))
            break;
    }
    *result = std::move(fixedResult);
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
