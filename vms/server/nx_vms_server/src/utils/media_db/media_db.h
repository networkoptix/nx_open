#ifndef __MEDIA_DB_H__
#define __MEDIA_DB_H__

#include <queue>
#include <cmath>

#include <boost/variant.hpp>
#include <QtCore>

#include "nx/utils/thread/mutex.h"
#include <nx/utils/log/log.h>
#include "nx/utils/thread/wait_condition.h"
#include "nx/utils/thread/long_runnable.h"
#include <recorder/device_file_catalog.h>
#include "media_file_operation.h"

// Refer to https://networkoptix.atlassian.net/wiki/display/SD/Proprietary+media+database+record+format
// for DB records format details.

namespace nx
{
namespace media_db
{

struct FileHeader
{
    quint64 part1;
    quint64 part2;

    static const int kSerializedRecordSize = sizeof(part1) + sizeof(part2);

    FileHeader() : part1(0), part2(0) {}

    uint8_t getDbVersion() const { return part1 & 0xff;  }
    void setDbVersion(uint8_t dbVersion) { part1 |= (dbVersion & 0xff); }
};

struct CameraOperation: RecordBase
{
    QByteArray cameraUniqueId;

    CameraOperation(quint64 i1 = 0, const QByteArray &ar = QByteArray())
        : RecordBase(i1),
          cameraUniqueId(ar)
    {}

    int getCameraUniqueIdLen() const;
    void setCameraUniqueIdLen(int len);

    int getCameraId() const;
    void setCameraId(int cameraId);

    QByteArray getCameraUniqueId() const;
    void setCameraUniqueId(const QByteArray &uniqueId);
};

inline bool operator==(const CameraOperation& c1, const CameraOperation& c2)
{
    return c1.part1 == c2.part1 && c1.cameraUniqueId == c2.cameraUniqueId;
}

typedef boost::variant<boost::blank, MediaFileOperation, CameraOperation> DBRecord;

class MediaDbReader
{
public:
    MediaDbReader(QIODevice* ioDevice);

    bool readFileHeader(uint8_t *dbVersion);
    DBRecord readRecord();

private:
    QDataStream m_stream;
};

using DBRecordQueue = std::queue<DBRecord>;

class MediaDbWriter: public QnLongRunnable
{
public:
    MediaDbWriter();
    ~MediaDbWriter();
    void setDevice(QIODevice* ioDevice);
    void writeRecord(const DBRecord &record);

    static bool writeFileHeader(QIODevice* ioDevice, uint8_t dbVersion);

private:
    QDataStream m_stream;
    mutable QnMutex m_mutex;
    QnWaitCondition m_waitCondition;
    DBRecordQueue m_queue;

    virtual void run() override;
    virtual void pleaseStop() override;
};

} // namespace media_db
} // namespace nx

#endif // __MEDIA_DB_TYPES_H__
