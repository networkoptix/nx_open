#ifndef __MOTION_WRITER_H__
#define __MOTION_WRITER_H__

#include <QRegion>
#include <QFile>
#include <QSharedPointer>
#include "core/resource/network_resource.h"
#include "core/datapacket/mediadatapacket.h"
#include "utils/media/sse_helper.h"
#include "recording/device_file_catalog.h"

static const int MOTION_INDEX_HEADER_SIZE = 16;
static const int MOTION_INDEX_RECORD_SIZE = 8;
static const int MOTION_DATA_RECORD_SIZE = MD_WIDTH*MD_HEIGHT/8;

class QnMotionArchive;

#pragma pack(push, 1)
struct IndexRecord
{
    int start;    // at ms since epoch + start offset from a file header
    int duration; // at ms
};

struct IndexHeader
{
    qint64 startTime;
    qint16 interval;
    qint8 version;
    char dummy[5];
};
#pragma pack(pop)

class QnMotionArchiveConnection
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

class QnMotionArchive
{
public:
    QnMotionArchive(QnNetworkResourcePtr resource);
    bool saveToArchive(QnMetaDataV1Ptr data);
    QnTimePeriodList mathPeriod(const QRegion& region, qint64 startTime, qint64 endTime);
    QnMotionArchiveConnectionPtr createConnection();

    static void createMask(const QRegion& region,  __m128i* mask, int& msMaskStart, int& msMaskEnd);
private:
    QString getFilePrefix(const QDateTime& datetime);
    void dateBounds(qint64 datetimeMs, qint64& minDate, qint64& maxDate);
    bool mathImage(const __m128i* data, const __m128i* mask, int maskStart, int maskEnd);
    void fillFileNames(qint64 datetimeMs, QFile* motionFile, QFile* indexFile);
    bool saveToArchiveInternal(QnMetaDataV1Ptr data);

    bool loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, qint64 msTime);

    friend class QnMotionArchiveConnection;
private:
    QnNetworkResourcePtr m_resource;
    qint64 m_lastDateForCurrentFile;
    
    QFile m_detailedMotionFile;
    QFile m_detailedIndexFile;
    qint64 m_firstTime;
    __m128i m_mask[MD_WIDTH * MD_HEIGHT / 128];
    QMutex m_fileAccessMutex;
    QnMetaDataV1Ptr m_lastDetailedData;
};

#endif // __MOTION_WRITER_H__
