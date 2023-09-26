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

static const char kVersion = 3;
static const int kReadBufferSize = 1024 * 256;
static const int kAlignment = 16;
static const int kIndexRecordSize = sizeof(IndexRecord);
static const int kMetadataIndexHeaderSize = sizeof(IndexHeader);

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

qint64 Index::dataOffset(int recordNumber, bool noGeometryMode) const
{
    return dataSize(0, recordNumber, noGeometryMode);
}

qint64 Index::dataSize(int from, int size, bool noGeometryMode) const
{
    qint64 counter = 0;
    for (int i = from; i < from + size; ++i)
        counter += records[i].recordCount();
    const int recordSize = noGeometryMode ? header.noGeometryRecordSize() : header.recordSize;
    return recordSize * counter;
}

void Index::reset()
{
    records.clear();
    header = IndexHeader();

    header.version = kVersion;
    header.recordSize = m_owner->baseRecordSize();
    header.intervalMs = m_owner->aggregationIntervalSeconds() * 1000;

    memset(header.dummy, 0, sizeof(header.dummy));
}

int64_t Index::truncateTo(qint64 totalMediaRecords)
{
    int64_t counter = 0;
    for (int i = 0; i < records.size(); ++i)
    {
        const int indexRecords = records[i].recordCount();
        if (counter + indexRecords > totalMediaRecords)
        {
            if (counter < totalMediaRecords)
            {
                NX_VERBOSE(this, "Truncating index record %1 from %2 to %3",
                    i, indexRecords, totalMediaRecords - counter);
                records[i].setRecordCount(totalMediaRecords - counter);
                counter += totalMediaRecords - counter;
                ++i;
                if (i == records.size())
                    break;
            }
            NX_VERBOSE(this, "Metadata index is truncated from %1 to %2 records",
                records.size(), i);
            records.erase(records.begin() + i, records.end());
            break;
        }
        counter += indexRecords;
    }
    return counter;
}

bool Index::load(const QDateTime& time)
{
    return load(time.toMSecsSinceEpoch());
}

bool Index::load(qint64 timestampMs)
{
    auto indexFile = MetadataBinaryFileFactory::instance().create();
    m_owner->fillFileNames(timestampMs, indexFile.get());
    if (!indexFile->openReadOnly())
        return false;
    return load(*indexFile);
}

bool Index::load(AbstractMetadataBinaryFile& indexFile)
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
    m_aggregationIntervalSeconds(aggregationIntervalSeconds),
    m_detailedMetadataFile(MetadataBinaryFileFactory::instance().create()),
    m_detailedIndexFile(MetadataBinaryFileFactory::instance().create()),
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

void MetadataArchive::fillFileNames(qint64 datetimeMs, AbstractMetadataBinaryFile* indexFile) const
{
    fillFileNames(datetimeMs, nullptr, false, indexFile);
}

void MetadataArchive::fillFileNames(qint64 datetimeMs, AbstractMetadataBinaryFile* metadataFile, bool noGeometry, AbstractMetadataBinaryFile* indexFile) const
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

bool resizeFile(AbstractMetadataBinaryFile* file, qint64 size)
{
    if (file->size() == size)
        return true;
    return file->resize(size);
}

bool MetadataArchive::openFiles(qint64 timestampMs)
{
    m_index.reset();
    dateBounds(timestampMs, m_firstTime, m_lastDateForCurrentFile);

    fillFileNames(timestampMs, m_detailedMetadataFile.get(), /*noGeometry*/ false, m_detailedIndexFile.get());

    auto dirName = getFilePrefix(QDateTime::fromMSecsSinceEpoch(timestampMs).date());
    QDir dir;
    dir.mkpath(dirName);

    if (!m_detailedMetadataFile->openRW())
        return false;

    if (!m_detailedIndexFile->openRW())
        return false;

    if (m_detailedIndexFile->size() == 0)
    {
        m_index.header.startTimeMs = m_firstTime;
        m_detailedIndexFile->write((const char*) &m_index.header, sizeof(IndexHeader));
    }
    else
    {
        m_index.load(*m_detailedIndexFile);
        m_firstTime = m_index.header.startTimeMs;
        if (m_index.records.size() > 0)
            m_lastRecordedTime = m_index.records.last().start + m_index.header.startTimeMs;
    }

    if (m_index.header.noGeometryModeSupported() && m_index.header.noGeometryRecordSize() > 0)
    {
        auto name = m_detailedMetadataFile->fileName();
        name = name.replace("detailed", "nogeometry");
        m_noGeometryMetadataFile = MetadataBinaryFileFactory::instance().create();
        m_noGeometryMetadataFile->setFileName(name);
        m_noGeometryMetadataFile->openRW();
    }

    // Truncate biggest file. So, it is error checking.
    int64_t availableMediaRecords = m_detailedMetadataFile->size() / m_index.header.recordSize;
    if (m_noGeometryMetadataFile)
    {
        availableMediaRecords = std::min(availableMediaRecords,
            (int64_t) m_noGeometryMetadataFile->size() / m_index.header.noGeometryRecordSize());
    }
    int64_t mediaRecords = m_index.truncateTo(availableMediaRecords);
    if (!resizeFile(m_detailedIndexFile.get(), m_index.indexFileSize()))
    {
        NX_WARNING(this, "Failed to resize file %1 to size %2",
            m_detailedIndexFile->fileName(), m_index.indexFileSize());
        return false;
    }

    if (!resizeFile(m_detailedMetadataFile.get(), mediaRecords * m_index.header.recordSize))
    {
        NX_WARNING(this, "Failed to resize file %1 to size %2",
            m_detailedMetadataFile->fileName(), mediaRecords * m_index.header.recordSize);
        return false;
    }
    if (m_noGeometryMetadataFile)
    {
        const int recordSize = m_index.header.noGeometryRecordSize();
        if (!resizeFile(m_noGeometryMetadataFile.get(), mediaRecords * recordSize))
        {
            NX_WARNING(this, "Failed to resize file %1 to size %2",
                m_noGeometryMetadataFile->fileName(), mediaRecords * recordSize);
            return false;
        }
        m_noGeometryMetadataFile->seek(m_noGeometryMetadataFile->size());
    }

    m_detailedMetadataFile->seek(m_detailedMetadataFile->size());
    m_detailedIndexFile->seek(m_detailedIndexFile->size());

    return true;
}

bool MetadataArchive::saveToArchiveInternal(const QnConstAbstractCompressedMetadataPtr& data)
{
    NX_MUTEX_LOCKER lock(&m_writeMutex);

    const int recordCount = data->dataSize() / m_index.header.recordSize;

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
            m_detailedIndexFile->seek(0);
            m_detailedIndexFile->write((const char*)&m_index.header, sizeof(IndexHeader));
            m_detailedIndexFile->seek(m_detailedIndexFile->size());
        }
    }

    /**
     * It is possible that timestamp < m_firstTime in case of time has been changed to the past
     * and timezone (or DST flag) has been changed at the same time.
     */
    quint32 relTime = quint32(qMax(0LL, timestamp - m_firstTime));
    quint32 duration = std::max(kMinimalDurationMs, quint32(data->m_duration / 1000));
    NX_ASSERT(data->dataSize() % m_index.header.recordSize == 0);

    if (m_noGeometryMetadataFile)
    {
        const char* dataEx = data->data() + kGeometrySize;
        const int recordExSize = m_index.header.recordSize - kGeometrySize;
        for (int i = 0; i < recordCount; ++i)
        {
            if (m_noGeometryMetadataFile->write(dataEx, recordExSize) != recordExSize)
                NX_WARNING(this, "Failed to write nogeometry file for camera %1", m_physicalId);
            dataEx += m_index.header.recordSize;
        }
    }

    if (m_detailedIndexFile->write((const char*)&relTime, 4) != 4)
    {
        NX_WARNING(this, "Failed to write index file for camera %1", m_physicalId);
    }
    quint32 durationAndRecordCount = (duration & 0xffffff) + ((recordCount-1) << 24);
    if (m_detailedIndexFile->write((const char*)&durationAndRecordCount, 4) != 4)
    {
        NX_WARNING(this, "Failed to write index file for camera %1", m_physicalId);
    }

    if (m_detailedMetadataFile->write(data->data(), data->dataSize())
        != static_cast<qint64>(data->dataSize()))
    {
        NX_WARNING(this, "Failed to write index file for camera %1", m_physicalId);
    }

    m_detailedMetadataFile->flush();
    if (m_noGeometryMetadataFile)
        m_noGeometryMetadataFile->flush();
    m_detailedIndexFile->flush();

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
    AbstractMetadataBinaryFile& metadataFile,
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

    const int recordSize = recordMatcher->isNoGeometryMode()
        ? index.header.noGeometryRecordSize() : index.header.recordSize;

    int indexRecordNumber = startItr - index.records.begin();
    qint64 dataOffset = index.dataOffset(indexRecordNumber, recordMatcher->isNoGeometryMode());
    metadataFile.seek(dataOffset);

    QVector<IndexRecord>::const_iterator i = startItr;
    const int maxRecordsInBuffer = kReadBufferSize / recordSize;
    int mediaRecordsPerIndexRecord = i->recordCount();
    while (mediaRecordsLeft > 0)
    {
        const int recordsToRead = qMin(mediaRecordsLeft, maxRecordsInBuffer);
        const int bufferSize = recordsToRead * recordSize;
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
            if (checkPeriod(filter, fullStartTimeMs, i->duration()))
            {
                if (curData + recordSize > dataEnd)
                {
                    NX_DEBUG(this, "Unexpected metadata file size");
                    break;
                }
                if (recordMatcher->matchRecord(fullStartTimeMs, curData, recordSize))
                {
                if (!addRecordFunc(recordMatcher->filter(), fullStartTimeMs, i->duration(), rez))
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
        if (!checkPeriod(filter, fullStartTimeMs, i->duration()))
            continue;
        if (!addRecordFunc(filter, fullStartTimeMs, i->duration(), rez))
            return;
        if (interruptionCallback && interruptionCallback())
            return;
    }
}

void MetadataArchive::loadDataFromIndexDesc(
    AddRecordFunc addRecordFunc,
    RecordMatcher* recordMatcher,
    std::function<bool()> interruptionCallback,
    AbstractMetadataBinaryFile& metadataFile,
    const Index& index,
    QVector<IndexRecord>::iterator startItr,
    const QVector<IndexRecord>::iterator endItr,
    QnTimePeriodList& rez) const
{
    nx::utils::ByteArray buffer(kAlignment, 0, 0);

    const int recordSize = recordMatcher->isNoGeometryMode()
        ? index.header.noGeometryRecordSize() : index.header.recordSize;

    int mediaRecordsLeft = 0;
    for (auto i = startItr; i < endItr; ++i)
        mediaRecordsLeft += i->recordCount();
    if (mediaRecordsLeft == 0)
        return;

    const int maxRecordsInBuffer = kReadBufferSize / recordSize;
    const int recordsToRead = qMin(mediaRecordsLeft, maxRecordsInBuffer);

    auto calcRecordNumberToSeek =
        [&index](auto currentItr, auto startItr, int maxMediaRecords)
        {
            int mediaRecordsInBlock = 0;
            while (currentItr >= startItr)
            {
                const int records = currentItr->recordCount();
                if (mediaRecordsInBlock + records > maxMediaRecords)
                    break;
                mediaRecordsInBlock += records;
                --currentItr;
            }

            return mediaRecordsInBlock;
        };

    // math file (one month)
    QVector<IndexRecord>::const_iterator i = endItr - 1;
    int64_t mediaFileOffset = index.dataOffset(endItr - index.records.begin(), recordMatcher->isNoGeometryMode());
    while (mediaRecordsLeft > 0)
    {
        int mediaRecordsPerIndexRecord = i->recordCount();
        int recordsToRead = std::min(mediaRecordsLeft, maxRecordsInBuffer);
        int actualRecordsToRead = calcRecordNumberToSeek(i, startItr, recordsToRead);
        mediaFileOffset -= actualRecordsToRead * recordSize;
        metadataFile.seek(mediaFileOffset);

        int mediaDataSize = actualRecordsToRead * recordSize;
        buffer.reserve(mediaDataSize);
        const int read = metadataFile.read(buffer.data(), mediaDataSize);
        if (read <= 0)
            break;

        quint8* dataEnd = (quint8*) buffer.data() + read;
        quint8* curData = dataEnd - recordSize;
        const auto filter = recordMatcher->filter();
        while (i >= startItr && curData >= (quint8*) buffer.data())
        {
            qint64 fullStartTimeMs = i->start + index.header.startTimeMs;
            if (checkPeriod(filter, fullStartTimeMs, i->duration()))
            {
                if (recordMatcher->matchRecord(fullStartTimeMs, curData, recordSize))
                {
                    if (!addRecordFunc(filter, fullStartTimeMs, i->duration(), rez))
                        return;
                }
                if (interruptionCallback && interruptionCallback())
                    return;
            }

            --mediaRecordsLeft;
            curData -= recordSize;
            if (--mediaRecordsPerIndexRecord == 0)
            {
                if (--i >= startItr)
                    mediaRecordsPerIndexRecord = i->recordCount();
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
        if (!checkPeriod(filter, fullStartTimeMs, i->duration()))
            continue;

        if (!addRecordFunc(filter, fullStartTimeMs, i->duration(), rez))
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

        auto metadataFile = MetadataBinaryFileFactory::instance().create();
        Index index(this);

        if (index.load(timePointMs))
        {
            const bool hasDiscontinue = index.header.flags & IndexHeader::Flags::hasDiscontinue;
            needToFixDiscontinuity |= hasDiscontinue;

            const bool noGeometryMode = index.header.noGeometryModeSupported()
                && index.header.noGeometryRecordSize() > 0
                && recordMatcher->isWholeFrame();
            recordMatcher->setNoGeometryMode(noGeometryMode);
            fillFileNames(timePointMs, metadataFile.get(), noGeometryMode, nullptr);
            NX_VERBOSE(this, nx::format("Matching metadata periods for camera %1 for month %2")
                .args(m_physicalId, metadataFile->fileName()));

            if (recordMatcher->isEmpty() || metadataFile->openReadOnly())
            {
                const int recordSize = noGeometryMode
                    ? index.header.noGeometryRecordSize() : index.header.recordSize;
                if (metadataFile->isOpen())
                    index.truncateTo(metadataFile->size() / recordSize);

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
                            if (qBetween(prevItr->start, relativeStartTime, prevItr->start + prevItr->duration()))
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
                            *metadataFile, index, startItr, endItr,
                            rez);
                    }
                    else
                    {
                        loadDataFromIndex(
                            hasDiscontinue ? addToResultAscUnordered : addToResultAsc,
                            recordMatcher, breakCallback,
                            *metadataFile, index, startItr, endItr,
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
