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

constexpr uint8_t kDbVersion = 1;

struct FileHeader
{
    quint64 part1;
    quint64 part2;

    static const int kSerializedRecordSize = sizeof(part1) + sizeof(part2);

    FileHeader() : part1(0), part2(0) {}

    uint8_t getDbVersion() const { return part1 & 0xff;  }
    void setDbVersion(uint8_t dbVersion) { part1 |= (dbVersion & 0xff); }

    void serialize(ByteStreamWriter& writer) const
    {
        writer.writeUint64(part1);
        writer.writeUint64(part2);
    }

    bool deserialize(ByteStreamReader* reader)
    {
        if (!reader->hasBuffer(kSerializedRecordSize))
            return false;

        part1 = reader->readUint64();
        part2 = reader->readUint64();

        return true;
    }
};

struct CameraOperation : RecordBase
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

    int serializedRecordSize() const
    {
        return cameraUniqueId.size() + kSerializedRecordSize;
    }

    void serialize(ByteStreamWriter& writer) const
    {
        writer.writeUint64(part1);
        writer.writeRawData(cameraUniqueId.data(), cameraUniqueId.size());
    }
};

inline bool operator==(const CameraOperation& c1, const CameraOperation& c2)
{
    return c1.part1 == c2.part1 && c1.cameraUniqueId == c2.cameraUniqueId;
}

typedef boost::variant<boost::blank, MediaFileOperation, CameraOperation> DBRecord;

class DbReader
{
public:
    struct RemoveData
    {
        quint64 hash = 0; //< MediaFileOperation::hashInCatalog(). Just timestamp.
        size_t removeToNumber = 0; //< Remove records till number.

        bool operator<(const RemoveData& right) const
        {
            if (hash != right.hash)
                return hash < right.hash;
            return removeToNumber < right.removeToNumber;
        }

        bool operator==(const RemoveData& right) const
        {
            return hash == right.hash && removeToNumber == right.removeToNumber;
        }
    };

    struct Data
    {
        FileHeader header;
        std::vector<CameraOperation> cameras;

        std::unordered_map<int, std::vector<MediaFileOperation>> addRecords; //< key: cameraId*2 + catalog
        std::unordered_map<int, std::vector<RemoveData>> removeRecords;
    };

    static bool parse(const QByteArray& fileContent, Data* parsedData);

private:
    static bool deserialize(const QByteArray& buffer, Data* parsedData);
};

using DBRecordQueue = std::queue<DBRecord>;

class MediaDbWriter
{
public:
    MediaDbWriter();
    void setDevice(QIODevice* ioDevice);
    void writeRecord(const DBRecord &record);

    static bool writeFileHeader(QIODevice* ioDevice, uint8_t dbVersion);

private:
    QDataStream m_stream;
};

} // namespace media_db
} // namespace nx

#endif // __MEDIA_DB_TYPES_H__
