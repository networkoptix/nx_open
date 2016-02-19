#ifndef __MEDIA_DB_H__
#define __MEDIA_DB_H__

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <boost/variant.hpp>

namespace nx
{
namespace media_db
{

struct FileHeader
{
    qint64 byte_1;
    qint64 byte_2;

    FileHeader() : byte_1(0), byte_2(0) {}

    uint8_t getDbVersion() const { return byte_1 & 0xff;  }
    void setDbVersion(uint8_t dbVersion) { byte_1 |= (dbVersion & 0xff); }
};

enum class RecordType
{
    FileOperationAdd = 0,
    FileOperationDelete = 1,
    CameraOperationAdd = 2,
};

struct RecordBase
{
    qint64 byte_1;
    
    RecordType recordType() const {}
};

struct MediaFileOperation : RecordBase
{
    qint64 byte_2;
};

struct CameraOperation : RecordBase
{
    QString cameraUniqueId;
};

typedef boost::variant<MediaFileOperation, CameraOperation> WriteRecordType;

enum class Error
{
    NoError,
    ReadError,
    WriteError,
    ParseError,
    Eof
};

class DbHelperHandler
{
public:
    virtual ~DbHelperHandler() {}

    virtual void handleCameraRead(const CameraOperation &cameraOp, Error error) = 0;
    virtual void handleMediaFileRead(const MediaFileOperation &mediaFileOp, Error error) = 0;

    virtual void handleRecordWrite(Error error) = 0;
};

class DbHelper
{
    typedef std::queue<WriteRecordType> WriteQueue;
public:
    DbHelper(QIODevice *device, DbHelperHandler *const handler);
    ~DbHelper();

    Error readFileHeader(uint8_t *dbVersion);
    Error writeFileHeader(uint8_t dbVersion);

    Error readRecord();
    void writeRecordAsync(const WriteRecordType &record);

private:
    void startWriter();
    void stopWriter();

    Error getError() const;
private:
    QIODevice *m_device;
    DbHelperHandler *m_handler;
    WriteQueue m_writeQueue;
    QDataStream m_stream;

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    bool m_needStop;
};

} // namespace media_db
} // namespace nx

#endif // __MEDIA_DB_TYPES_H__