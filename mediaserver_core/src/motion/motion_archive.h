#ifndef __MOTION_WRITER_H__
#define __MOTION_WRITER_H__

#include <QtGui/QRegion>
#include <QtCore/QFile>
#include <QSharedPointer>
#include "nx/streaming/media_data_packet.h"
#include "utils/media/sse_helper.h"
#include "recorder/device_file_catalog.h"
#include <core/resource/resource_fwd.h>
#include "motion/abstract_motion_archive.h"

#include <motion/motion_detection.h>

static const int MOTION_INDEX_HEADER_SIZE = 16;
static const int MOTION_INDEX_RECORD_SIZE = 8;
static const int MOTION_DATA_RECORD_SIZE = Qn::kMotionGridWidth*Qn::kMotionGridHeight/8;

class QnMotionArchive;

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

class QnMotionArchiveConnection: public QnAbstractMotionArchiveConnection
{
public:
    QnMetaDataV1Ptr getMotionData(qint64 timeUsec);
    virtual ~QnMotionArchiveConnection();
private:
    QnMotionArchiveConnection(QnMotionArchive* owner);

    friend class QnMotionArchive;
private:
    QnMotionArchive* m_owner;
    qint64 m_minDate;
    qint64 m_maxDate;
    QVector<IndexRecord> m_index;
    IndexHeader m_indexHeader;
    QFile m_motionFile;

    int m_motionLoadedStart;
    int m_motionLoadedEnd;

    qint64 m_lastTimeMs;
    QVector<IndexRecord>::iterator m_indexItr;
    quint8* m_motionBuffer;
    QnMetaDataV1Ptr m_lastResult;
};

typedef QSharedPointer<QnMotionArchiveConnection> QnMotionArchiveConnectionPtr;

class QnMotionArchive: public QObject
{
    Q_OBJECT
public:
    QnMotionArchive(QnNetworkResourcePtr resource, int channel);
    virtual ~QnMotionArchive();
    bool saveToArchive(QnConstMetaDataV1Ptr data);
    QnTimePeriodList matchPeriod(const QRegion& region, qint64 startTime, qint64 endTime, int detailLevel);
    QnMotionArchiveConnectionPtr createConnection();

    qint64 minTime() const;
    qint64 maxTime() const;
    int getChannel() const;

private:
    QString getFilePrefix(const QDate& datetime) const;
    void dateBounds(qint64 datetimeMs, qint64& minDate, qint64& maxDate) const;
    void fillFileNames(qint64 datetimeMs, QFile* motionFile, QFile* indexFile) const;
    bool saveToArchiveInternal(QnConstMetaDataV1Ptr data);
    QString getChannelPrefix() const;

    bool loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, const QDateTime& time) const;
    bool loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, const QDate& time) const;
    bool loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, qint64 msTime) const;
    bool loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, QFile& indexFile) const;

    void loadRecordedRange();
    int getSizeForTime(qint64 timeMs, bool reloadIndex);

    friend class QnMotionArchiveConnection;
private:
    QnNetworkResourcePtr m_resource;
    int m_channel;
    QnSecurityCamResourcePtr m_camResource;
    qint64 m_lastDateForCurrentFile;

    QFile m_detailedMotionFile;
    QFile m_detailedIndexFile;
    qint64 m_firstTime;
    QnMutex m_writeMutex;
    QnMetaDataV1Ptr m_lastDetailedData;

    std::atomic<qint64> m_minMotionTime;
    std::atomic<qint64> m_maxMotionTime;
    qint64 m_lastRecordedTime;
    qint64 m_lastTimestamp;
    int m_middleRecordNum;
    QVector<IndexRecord> m_index;
    IndexHeader m_indexHeader;
};

#endif // __MOTION_WRITER_H__
