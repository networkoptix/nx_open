// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QVector>

#include <nx/media/meta_data_packet.h>
#include <nx/utils/thread/mutex.h>
#include <recording/time_period_list.h>

#include "dao/qfile_metadata_binary_file.h"

namespace nx::vms::metadata {

static const int kGeometrySize = Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8;

#pragma pack(push, 1)
    struct IndexRecord
    {
        quint32 start = 0;    //< Relative time in milliseconds as an offset from a file header.
        quint16 duration;
        quint8 extraWords = 0;
        quint8 recCount = 0;
        //< Record duration. Up to 4.5 hours.
        quint8 recordCount() const { return recCount + 1; }
        void setRecordCount(quint8 recordCount) { recCount = recordCount - 1; }
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

        // Multiplier for the variable 'extraWords'.
        // Record size is calculated as: baseRecordSize + extraWords * wordSize.
        quint8 wordSize = 0;
        char dummy[1];

        int noGeometryRecordSize() const { return recordSize - kGeometrySize; }
        bool noGeometryModeSupported() const { return version >= 3; }
        bool variousRecordSizeSupported() const { return version >= 4; }

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
    bool load(AbstractMetadataBinaryFile& indexFile);

    qint64 mediaFileSize(bool noGeometryMode) const;
    qint64 dataOffset(int recordNumber, bool noGeometryMode) const;

    /*
     * Calculate buffer size to read mediaRecords.
     * Buffer is rounded up to the one or more mediaRecord size
     * according to the index. It could be rounded up to the several
     * media records to cover full index record in case of index record
     * contains several media records.
     */
    qint64 mediaSize(int fromIndex, int suggestedbufferSize, bool noGeometryMode) const;
    qint64 mediaSizeDesc(int from, int suggestedbufferSize, bool noGeometryMode) const;
    qint64 extraRecordSize(int index) const;

    void reset();
    bool truncateToBytes(qint64 dataSize, bool noGeometryMode);
    bool updateTail(QFile* indexFile);
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

    virtual void onFileOpened(uint64_t /*timePointMs*/, const IndexHeader& /*header*/) {}
    virtual bool isNoData() const { return false; }

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
    int wordSize() const;
    int getChannel() const;
    int aggregationIntervalSeconds() const;

    QString physicalId() const;
    QString getFilePrefix(const QDate& datetime) const;
    bool supportVariousRecordSize(std::chrono::milliseconds timestamp);


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
    void fillFileNames(qint64 datetimeMs, AbstractMetadataBinaryFile* motionFile, bool noGeometry, AbstractMetadataBinaryFile* indexFile) const;
    void fillFileNames(qint64 datetimeMs, AbstractMetadataBinaryFile* indexFile) const;
    bool saveToArchiveInternal(const QnConstAbstractCompressedMetadataPtr& data, int extraWords);
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
        AbstractMetadataBinaryFile& motionFile,
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
        AbstractMetadataBinaryFile& motionFile,
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

protected:
    bool openFiles(qint64 timestampMs);

private:
    QString m_filePrefix;
    int m_recordSize = 0;
    int m_wordSize = 0;
    int m_aggregationIntervalSeconds = 0;

    std::shared_ptr<AbstractMetadataBinaryFile> m_detailedMetadataFile;
    std::unique_ptr<AbstractMetadataBinaryFile> m_noGeometryMetadataFile;
    std::shared_ptr<AbstractMetadataBinaryFile> m_detailedIndexFile;

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
