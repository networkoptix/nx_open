// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QVector>

#include <nx/utils/thread/mutex.h>
#include <nx/streaming/media_data_packet.h>

#include <recording/time_period_list.h>

namespace nx::vms::metadata {

#pragma pack(push, 1)
    struct IndexRecord
    {
        quint32 start = 0;    //< Relative time in milliseconds as an offset from a file header.
        quint32 durationAndExtraWords = 0;

        //< Record duration. Up to 4.5 hours.
        quint32 duration() const { return durationAndExtraWords & 0xffffff; }
        quint8 extraWords() const { return durationAndExtraWords >> 24; }
    };

    struct IndexHeader
    {
        qint64 startTimeMs = 0;
        qint16 intervalMs = 0;
        qint8 version = 0;

        // Base size of the each record in bytes.
        quint16 baseRecordSize = 0;
        // Multiplier for the variable 'extraWords'.
        // Record size is calculated as: baseRecordSize + extraWords * wordSize.
        quint8 wordSize = 0;

        char dummy[2];
    };
#pragma pack(pop)

inline bool operator < (const IndexRecord& first, const IndexRecord& other) { return first.start < other.start; }
inline bool operator < (qint64 start, const IndexRecord& other) { return start < other.start; }
inline bool operator < (const IndexRecord& other, qint64 start) { return other.start < start; }

class MetadataArchive;

struct NX_VMS_COMMON_API Index
{
    Index(MetadataArchive* owner);

    bool load(const QDateTime& time);
    bool load(qint64 timestampMs);
    bool load(QFile& indexFile);

    qint64 dataOffset(int recordNumber) const;
    qint64 dataSize(int from, int size) const;
    qint64 dataSize(const QVector<IndexRecord>::const_iterator& itr) const;
    qint64 dataFileSize() const;
    qint64 indexFileSize() const;
    void reset();
    void truncate(qint64 dataFileSize);

    IndexHeader header;
    QVector<IndexRecord> records;
    MetadataArchive* m_owner = nullptr;
};

struct NX_VMS_COMMON_API Filter
{
    std::chrono::milliseconds startTime{ 0 };
    std::chrono::milliseconds endTime{ 0 };
    std::chrono::milliseconds detailLevel = std::chrono::milliseconds::zero();
    int limit = -1;
    Qt::SortOrder sortOrder = Qt::SortOrder::AscendingOrder;
};

class NX_VMS_COMMON_API RecordMatcher
{
public:
    RecordMatcher(const Filter* filter): m_filter(filter) {}

    const Filter* filter() const { return m_filter; }
    virtual bool matchRecord(int64_t timestampMs, const uint8_t* data, int recordSize) const = 0;
private:
    const Filter* m_filter;
};

class NX_VMS_COMMON_API MetadataArchive: public QObject
{
public:
    MetadataArchive(
        const QString& filePrefix,
        int baseRecordSize,
        int wordSize,
        int aggregationIntervalSeconds,
        const QString& dataDir,
        const QString& physicalId,
        int channel);

    int baseRecordSize() const;
    int wordSize() const;
    int getChannel() const;
    qint64 minTime() const;
    qint64 maxTime() const;
    int aggregationIntervalSeconds() const;

    QString physicalId() const;

    static constexpr quint32 kMinimalDurationMs = 125;
protected:
    friend struct Index;

    using MatchExtraDataFunc =
        std::function<bool(int64_t timestampMs, const Filter&, const quint8*, int)>;

    QnTimePeriodList matchPeriodInternal(
        RecordMatcher* recordMatcher,
        std::function<bool()> interruptionCallback = nullptr);

    QString getFilePrefix(const QDate& datetime) const;
    void dateBounds(qint64 datetimeMs, qint64& minDate, qint64& maxDate) const;
    void fillFileNames(qint64 datetimeMs, QFile* motionFile, QFile* indexFile) const;
    bool saveToArchiveInternal(const QnConstAbstractCompressedMetadataPtr& data);
    QString getChannelPrefix() const;

    void loadRecordedRange();

private:
    int getSizeForTime(qint64 timeMs, bool reloadIndex);

    void loadDataFromIndex(
        RecordMatcher* recordMatcher,
        std::function<bool()> interruptionCallback,
        QFile& motionFile,
        const Index& index,
        QVector<IndexRecord>::iterator startItr,
        const QVector<IndexRecord>::iterator endItr,
        QnTimePeriodList& rez) const;
    void loadDataFromIndexDesc(
        RecordMatcher* recordMatcher,
        std::function<bool()> interruptionCallback,
        QFile& motionFile,
        const Index& index,
        QVector<IndexRecord>::iterator startItr,
        const QVector<IndexRecord>::iterator endItr,
        QnTimePeriodList& rez) const;
    bool openFiles(qint64 timestampMs);
private:
    QString m_filePrefix;
    int m_baseRecordSize = 0;
    int m_wordSize = 0;
    int m_aggregationIntervalSeconds = 0;

    QFile m_detailedMetadataFile;
    QFile m_detailedIndexFile;

    qint64 m_lastDateForCurrentFile = -1;

    qint64 m_firstTime = 0;
    nx::Mutex m_writeMutex;

    std::atomic<qint64> m_minMetadataTime{0};
    std::atomic<qint64> m_maxMetadataTime{0};
    qint64 m_lastRecordedTime = 0;
    int m_middleRecordNum = -1;

    Index m_index;

protected:
    const QString m_dataDir;
    const QString m_physicalId;
    const int m_channel;
};

} // namespace nx::vms::metadata
