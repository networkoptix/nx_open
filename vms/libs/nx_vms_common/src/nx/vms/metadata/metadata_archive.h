// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QVector>

#include <nx/media/meta_data_packet.h>
#include <nx/utils/thread/mutex.h>
#include <recording/time_period_list.h>

namespace nx::vms::metadata {

static const int kGeometrySize = Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8;

#pragma pack(push, 1)
    struct IndexRecord
    {
        quint32 start = 0;    //< Relative time in milliseconds as an offset from a file header.
        quint32 durationAndRecCount = 0;
        //< Record duration. Up to 4.5 hours.
        quint32 duration() const { return durationAndRecCount & 0xffffff; }
        quint8 recordCount() const { return (durationAndRecCount >> 24) + 1; }
        void setRecordCount(quint8 recordCount)
        {
            durationAndRecCount = (durationAndRecCount & 0xffffff) + ((recordCount - 1) << 24);
        }
    };

    struct IndexHeader
    {
        enum Flags
        {
            noFlags = 0,
            hasDiscontinue = 1
        };

        qint64 startTimeMs = 0;
        qint16 intervalMs = 0;
        qint8 version = 0;

        // Base size of the each record in bytes.
        quint16 recordSize = 0;

        quint8 flags = 0;
        char dummy[2];

        int noGeometryRecordSize() const { return recordSize - kGeometrySize; }
        bool noGeometryModeSupported() const { return version >= 3; }

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

    qint64 dataOffset(int recordNumber, bool noGeometryMode) const;
    qint64 dataSize(int from, int size, bool noGeometryMode) const;

    void reset();
    int64_t truncateTo(qint64 mediaRecords);
    qint64 indexFileSize() const;

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
    virtual bool isWholeFrame() const = 0;
    virtual bool matchRecord(int64_t timestampMs, const uint8_t* data, int recordSize) const = 0;
    virtual bool isEmpty() const = 0;

    void setNoGeometryMode(bool value) { m_noGeometryMode = value; }
    bool isNoGeometryMode() const { return m_noGeometryMode; }
private:
    const Filter* m_filter;
    bool m_noGeometryMode = false;
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
    int getChannel() const;
    int aggregationIntervalSeconds() const;

    QString physicalId() const;
    QString getFilePrefix(const QDate& datetime) const;

    static constexpr quint32 kMinimalDurationMs = 125;
protected:
    friend struct Index;

    using MatchExtraDataFunc =
        std::function<bool(int64_t timestampMs, const Filter&, const quint8*, int)>;
    using FixDiscontinue =
        std::function<void(QnTimePeriodList*, bool, const Filter*)>;

    static void onDiscontinueInMatchedResult(QnTimePeriodList* result, bool descendingOrder, const Filter* filter);

    QnTimePeriodList matchPeriodInternal(
        RecordMatcher* recordMatcher,
        std::function<bool()> interruptionCallback = nullptr,
        FixDiscontinue fixDiscontinue = onDiscontinueInMatchedResult);

    void dateBounds(qint64 datetimeMs, qint64& minDate, qint64& maxDate) const;
    void fillFileNames(qint64 datetimeMs, QFile* motionFile, bool noGeometry, QFile* indexFile) const;
    void fillFileNames(qint64 datetimeMs, QFile* indexFile) const;
    bool saveToArchiveInternal(const QnConstAbstractCompressedMetadataPtr& data);
    QString getChannelPrefix() const;

    template <typename T>
    static void sortData(T* data, bool descendingOrder)
    {
        auto ascComparator =
            [](const typename T::value_type& left, const typename T::value_type& right)
        {
            return left.startTimeMs < right.startTimeMs;
        };
        auto descComparator =
            [](const typename T::value_type& left, const typename T::value_type& right)
        {
            return left.startTimeMs > right.startTimeMs;
        };
        std::sort(data->begin(), data->end(), descendingOrder ? descComparator : ascComparator);
    }

private:

    using AddRecordFunc = std::function<bool(const Filter*, qint64, qint32, QnTimePeriodList&)>;

    void loadDataFromIndex(
        AddRecordFunc addRecordFunc,
        RecordMatcher* recordMatcher,
        std::function<bool()> interruptionCallback,
        QFile& motionFile,
        const Index& index,
        QVector<IndexRecord>::iterator startItr,
        const QVector<IndexRecord>::iterator endItr,
        QnTimePeriodList& rez) const;
    void loadDataFromIndex(
        AddRecordFunc addRecordFunc,
        const Filter* filter,
        std::function<bool()> interruptionCallback,
        const Index& index,
        QVector<IndexRecord>::iterator startItr,
        const QVector<IndexRecord>::iterator endItr,
        QnTimePeriodList& rez) const;

    void loadDataFromIndexDesc(
        AddRecordFunc addRecordFunc,
        RecordMatcher* recordMatcher,
        std::function<bool()> interruptionCallback,
        QFile& motionFile,
        const Index& index,
        QVector<IndexRecord>::iterator startItr,
        const QVector<IndexRecord>::iterator endItr,
        QnTimePeriodList& rez) const;
    void loadDataFromIndexDesc(
        AddRecordFunc addRecordFunc,
        const Filter* filter,
        std::function<bool()> interruptionCallback,
        const Index& index,
        QVector<IndexRecord>::iterator startItr,
        const QVector<IndexRecord>::iterator endItr,
        QnTimePeriodList& rez) const;

    bool openFiles(qint64 timestampMs);
private:
    QString m_filePrefix;
    int m_recordSize = 0;
    int m_aggregationIntervalSeconds = 0;

    QFile m_detailedMetadataFile;
    std::unique_ptr<QFile> m_noGeometryMetadataFile;
    QFile m_detailedIndexFile;

    qint64 m_lastDateForCurrentFile = -1;

    qint64 m_firstTime = 0;
    nx::Mutex m_writeMutex;

    qint64 m_lastRecordedTime = AV_NOPTS_VALUE;

    Index m_index;

protected:
    const QString m_dataDir;
    const QString m_physicalId;
    const int m_channel;
};

} // namespace nx::vms::metadata
