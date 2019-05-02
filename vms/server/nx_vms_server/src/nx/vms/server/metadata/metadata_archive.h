#pragma once

#include <QtGui/QRegion>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QVector>
#include "nx/streaming/media_data_packet.h"
#include <recording/time_period_list.h>

namespace nx::vms::server::metadata {

#pragma pack(push, 1)
    struct IndexRecord
    {
        quint32 start;    // at ms since epoch + start offset from a file header
        quint32 duration; // at ms
    };

    struct IndexHeader
    {
        qint64 startTime;
        qint16 interval;
        qint8 version;
        char dummy[5];
    };
#pragma pack(pop)

inline bool operator < (const IndexRecord& first, const IndexRecord& other) { return first.start < other.start; }
inline bool operator < (qint64 start, const IndexRecord& other) { return start < other.start; }
inline bool operator < (const IndexRecord& other, qint64 start) { return other.start < start; }

class MetadataArchive: public QObject
{
public:
    MetadataArchive(
        const QString& filePrefix,
        int recordSize,
        int aggregationIntervalSeconds,
        const QString& dataDir,
        const QString& uniqueId,
        int channel);

    int recordSize() const;
    int getChannel() const;
    qint64 minTime() const;
    qint64 maxTime() const;
    int aggregationIntervalSeconds() const;

    struct Filter
    {
        QRegion region;
        std::chrono::milliseconds startTime{0};
        std::chrono::milliseconds endTime{0};
        std::chrono::milliseconds detailLevel = std::chrono::milliseconds::zero();
        int limit = -1;
        Qt::SortOrder sortOrder = Qt::SortOrder::AscendingOrder;
    };

    static constexpr quint32 kMinimalDurationMs = 125;
protected:
    static const int kGridDataSize = Qn::kMotionGridWidth * Qn::kMotionGridHeight / 128;

    QnTimePeriodList matchPeriodInternal(const Filter& filter);
    QString getFilePrefix(const QDate& datetime) const;
    void dateBounds(qint64 datetimeMs, qint64& minDate, qint64& maxDate) const;
    void fillFileNames(qint64 datetimeMs, QFile* motionFile, QFile* indexFile) const;
    bool saveToArchiveInternal(const QnAbstractCompressedMetadataPtr& data);
    QString getChannelPrefix() const;

    void loadRecordedRange();

    bool loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, const QDateTime& time) const;
    bool loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, const QDate& time) const;
    bool loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, qint64 msTime) const;
    bool loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, QFile& indexFile) const;
protected:
    virtual bool matchAdditionData(const Filter& filter, const quint8* data, int size) { return true; }
private:
    int getSizeForTime(qint64 timeMs, bool reloadIndex);

    void loadDataFromIndex(
        const Filter& filter,
        QFile& motionFile,
        const IndexHeader& indexHeader,
        const QVector<IndexRecord>& index,
        QVector<IndexRecord>::iterator startItr,
        QVector<IndexRecord>::iterator endItr,
        quint8* buffer,
        simd128i* mask,
        int maskStart,
        int maskEnd,
        QnTimePeriodList& rez);
    void loadDataFromIndexDesc(
        const Filter& filter,
        QFile& motionFile,
        const IndexHeader& indexHeader,
        const QVector<IndexRecord>& index,
        QVector<IndexRecord>::iterator startItr,
        QVector<IndexRecord>::iterator endItr,
        quint8* buffer,
        simd128i* mask,
        int maskStart,
        int maskEnd,
        QnTimePeriodList& rez);
private:

    QString m_filePrefix;
    int m_recordSize = 0;
    int m_aggregationIntervalSeconds = 0;

    QFile m_detailedMetadataFile;
    QFile m_detailedIndexFile;

    qint64 m_lastDateForCurrentFile = 0;

    qint64 m_firstTime = 0;
    QnMutex m_writeMutex;

    std::atomic<qint64> m_minMetadataTime{0};
    std::atomic<qint64> m_maxMetadataTime{0};
    qint64 m_lastRecordedTime = 0;
    int m_middleRecordNum = -1;

    QVector<IndexRecord> m_index;
    IndexHeader m_indexHeader;
protected:
    QString m_dataDir;
    QString m_uniqueId;
    int m_channel;
};

} // namespace nx::vms::server::metadata
