#ifndef __MEDIA_DB_H__
#define __MEDIA_DB_H__

#include <queue>
#include <thread>
#include <cmath>

#include <boost/variant.hpp>
#include <QtCore>

#include "utils/thread/mutex.h"
#include "utils/thread/wait_condition.h"

// Refer to https://networkoptix.atlassian.net/wiki/display/SD/Proprietary+media+database+record+format 
// for DB records format details.

namespace nx
{
namespace media_db
{

struct FileHeader
{
    quint64 part_1;
    quint64 part_2;

    FileHeader() : part_1(0), part_2(0) {}

    uint8_t getDbVersion() const { return part_1 & 0xff;  }
    void setDbVersion(uint8_t dbVersion) { part_1 |= (dbVersion & 0xff); }
};

enum class RecordType
{
    FileOperationAdd = 0,
    FileOperationDelete = 1,
    CameraOperationAdd = 2,
};

enum class Mode
{
    Read,
    Write
};

inline quint64 getBitMask(int width) { return (quint64)std::pow(2, width) - 1; }

struct RecordBase
{
    quint64 part_1;
    
    RecordBase(quint64 i = 0) : part_1(i) {}
    RecordType getRecordType() const { return static_cast<RecordType>(part_1 & 0x3); }
    void setRecordType(RecordType recordType) { part_1 |= (quint64)recordType & 0x3; }
};

struct MediaFileOperation : RecordBase
{
    quint64 part_2;

    MediaFileOperation(quint64 i1 = 0, quint64 i2 = 0) : RecordBase(i1), part_2(i2) {}
    int getCameraId() const { return (part_1 >> 0x2) & 0xffff; }
    void setCameraId(int cameraId) { part_1 |= ((quint64)cameraId & 0xffff) << 0x2; }

    qint64 getStartTime() const { return (part_1 >> 0x12) & getBitMask(0x2all); }
    void setStartTime(qint64 startTime) { part_1 |= ((quint64)startTime & getBitMask(0x2all)) << 0x12; }

    int getDuration() const
    {
        quint64 p1 = (part_1 >> 0x3c) & 0xf;
        quint64 p2 = part_2 & getBitMask(0x10);
        return p2 | (p1 << 0x10);
    }
    void setDuration(int duration)
    {
        part_1 |= (((quint64)duration >> 0x10) & 0xf) << 0x3C;
        part_2 |= (quint64)duration & getBitMask(0x10);
    }

    int getTimeZone() const { return (part_2 >> 0x10) & getBitMask(0x7); }
    void setTimeZone(int timeZone) { part_2 |= ((quint64)timeZone & getBitMask(0x7)) << 0x10; }

    qint64 getFileSize() const { return (part_2 >> 0x17) & getBitMask(0x28ll); }
    void setFileSize(qint64 fileSize) { part_2 |= ((quint64)fileSize & getBitMask(0x28ll)) << 0x17; }

    int getCatalog() const { return (part_2 >> 0x3F) & 0x1; }
    void setCatalog(int catalog) { part_2 |= ((quint64)catalog & 0x1) << 0x3F; }
};

struct CameraOperation : RecordBase
{
    QByteArray cameraUniqueId;

    CameraOperation(quint64 i1 = 0, const QByteArray &ar = QByteArray()) 
        : RecordBase(i1),
          cameraUniqueId(ar)
    {}

    int getCameraUniqueIdLen() const { return (part_1 >> 0x2) & getBitMask(0xE); }
    void setCameraUniqueIdLen(int len) { part_1 |= ((quint64)len & getBitMask(0xE)) << 0x2; }

    int getCameraId() const { return (part_1 >> 0x10) & getBitMask(0x10); }
    void setCameraId(int cameraId) { part_1 |= ((quint64)cameraId & getBitMask(0x10)) << 0x10; }

    QByteArray getCameraUniqueId() const { return cameraUniqueId; }
    void setCameraUniqueId(const QByteArray &uniqueId) { cameraUniqueId = uniqueId; }
};

typedef boost::variant<MediaFileOperation, CameraOperation> WriteRecordType;

enum class Error
{
    NoError,
    ReadError,
    WriteError,
    ParseError,
    Eof,
    WrongMode
};

class DbHelperHandler
{
public:
    virtual ~DbHelperHandler() {}

    virtual void handleCameraOp(const CameraOperation &cameraOp, Error error) = 0;
    virtual void handleMediaFileOp(const MediaFileOperation &mediaFileOp, Error error) = 0;
    virtual void handleError(Error error) = 0;

    virtual void handleRecordWrite(Error error) = 0;
};

class DbHelper
{
    typedef std::queue<WriteRecordType> WriteQueue;
public:
    DbHelper(DbHelperHandler *const handler);
    ~DbHelper();

    DbHelper& operator = (DbHelper&& other) = default;

    Error readFileHeader(uint8_t *dbVersion);
    Error writeFileHeader(uint8_t dbVersion);

    Error readRecord();
    Error writeRecordAsync(const WriteRecordType &record);
    void stopWriter();
    void reset();

    void setMode(Mode mode);
    Mode getMode() const;

    QIODevice *getDevice() const;
    void setDevice(QIODevice *device);

private:
    void startWriter();
    Error getError() const;

private:
    QIODevice *m_device;
    DbHelperHandler *m_handler;
    WriteQueue m_writeQueue;
    QDataStream m_stream;

    std::thread m_thread;
    mutable QnMutex m_mutex;
    QnWaitCondition m_cond;
    QnWaitCondition m_writerDoneCond;
    bool m_needStop;
    Mode m_mode;
};

} // namespace media_db
} // namespace nx

#endif // __MEDIA_DB_TYPES_H__