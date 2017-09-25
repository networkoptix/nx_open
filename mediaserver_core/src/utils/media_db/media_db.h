#ifndef __MEDIA_DB_H__
#define __MEDIA_DB_H__

#include <deque>
#include <cmath>

#include <boost/variant.hpp>
#include <QtCore>

#include "nx/utils/thread/mutex.h"
#include <nx/utils/log/log.h>
#include "nx/utils/thread/wait_condition.h"
#include "nx/utils/thread/long_runnable.h"

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

    FileHeader() : part1(0), part2(0) {}

    uint8_t getDbVersion() const { return part1 & 0xff;  }
    void setDbVersion(uint8_t dbVersion) { part1 |= (dbVersion & 0xff); }
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
    quint64 part1;

    RecordBase(quint64 i = 0) : part1(i) {}
    RecordType getRecordType() const { return static_cast<RecordType>(part1 & 0x3); }
    void setRecordType(RecordType recordType) { part1 |= (quint64)recordType & 0x3; }
};

struct MediaFileOperation : RecordBase
{
    quint64 part2;

    MediaFileOperation(quint64 i1 = 0, quint64 i2 = 0) : RecordBase(i1), part2(i2) {}
    int getCameraId() const { return (part1 >> 0x2) & 0xffff; }
    void setCameraId(int cameraId) { part1 |= ((quint64)cameraId & 0xffff) << 0x2; }

    qint64 getStartTime() const
    {   // -1 is a special value which designates that this operation
        // should be done with entire catalog instead of one chunk.
        qint64 ret = (part1 >> 0x12) & getBitMask(0x2all);
        return ret == 1 ? -1 : ret;
    }

    void setStartTime(qint64 startTime)
    {
        startTime = startTime == -1 ? 1 : startTime;
        part1 |= ((quint64)startTime & getBitMask(0x2all)) << 0x12;
    }

    int getDuration() const
    {
        quint64 p1 = (part1 >> 0x3c) & 0xf;
        quint64 p2 = part2 & getBitMask(0x10);
        return p2 | (p1 << 0x10);
    }

    void setDuration(int duration)
    {
        part1 |= (((quint64)duration >> 0x10) & 0xf) << 0x3C;
        part2 |= (quint64)duration & getBitMask(0x10);
    }

    int getTimeZone() const
    {
        bool isPositive = ((part2 >> 0x10) & 0x1) == 0;

        int intPart = (part2 >> 0x11) & getBitMask(0x4);
        int remainder = (part2 >> 0x15) & 0x3;

        int result = intPart * 60;
        switch (remainder)
        {
        case 0:
            break;
        case 1:
            result += 30;
            break;
        case 2:
            result += 45;
            break;
        }

        return result * (isPositive ? 1 : -1);
    }

    void setTimeZone(int timeZone)
    {
        if (timeZone < 0)
            part2 |= 0x1ll << 0x10;

        timeZone = qAbs(timeZone);

        int intPart = timeZone / 60;
        part2 |= ((quint64)intPart & getBitMask(0x4)) << 0x11;

        int remainderPart = timeZone - intPart * 60;
        if (remainderPart == 30)
            part2 |= ((quint64)1 & 0x3) << 0x15;
        else if (remainderPart == 45)
            part2 |= ((quint64)2 & 0x3) << 0x15;
    }

    qint64 getFileSize() const { return (part2 >> 0x17) & getBitMask(0x27LL); }
    void setFileSize(qint64 fileSize) { part2 |= ((quint64)fileSize & getBitMask(0x27LL)) << 0x17; }

    int getFileTypeIndex() const;
    void setFileTypeIndex(int fileTypeIndex);

    int getCatalog() const { return (part2 >> 0x3F) & 0x1; }
    void setCatalog(int catalog) { part2 |= ((quint64)catalog & 0x1) << 0x3F; }
};

struct CameraOperation : RecordBase
{
    QByteArray cameraUniqueId;

    CameraOperation(quint64 i1 = 0, const QByteArray &ar = QByteArray())
        : RecordBase(i1),
          cameraUniqueId(ar)
    {}

    int getCameraUniqueIdLen() const { return (part1 >> 0x2) & getBitMask(0xE); }
    void setCameraUniqueIdLen(int len) { part1 |= ((quint64)len & getBitMask(0xE)) << 0x2; }

    int getCameraId() const { return (part1 >> 0x10) & getBitMask(0x10); }
    void setCameraId(int cameraId) { part1 |= ((quint64)cameraId & getBitMask(0x10)) << 0x10; }

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

class FaultTolerantDataStream
{
public:
    FaultTolerantDataStream(QIODevice* device):
        m_stream(device)
    {}

    template<typename T>
    FaultTolerantDataStream& operator<<(T t)
    {
        if (m_stream.device())
            m_stream << t;
        else
            NX_LOG("[media db] attempt to use null file stream", cl_logWARNING);

        return *this;
    }

    template<typename T>
    FaultTolerantDataStream& operator>>(T& t)
    {
        if (m_stream.device())
            m_stream >> t;
        else
            NX_LOG("[media db] attempt to use null file stream", cl_logWARNING);

        return *this;
    }

    void setDevice(QIODevice* device)
    {
        m_stream.setDevice(device);
    }

    QDataStream::Status status() const
    {
        return m_stream.status();
    }

    int writeRawData(const char* s, int len)
    {
        if (m_stream.device())
            return m_stream.writeRawData(s, len);

        NX_LOG("[media db] attempt to use null file stream", cl_logWARNING);
        return -1;
    }

    void setByteOrder(QDataStream::ByteOrder order)
    {
        m_stream.setByteOrder(order);
    }

    void resetStatus()
    {
        m_stream.resetStatus();
    }

    bool atEnd() const
    {
        if (!m_stream.device())
            NX_LOG("[media db] attempt to use null file stream", cl_logWARNING);

        return m_stream.atEnd();
    }

    int readRawData(char* s, int len)
    {
        if (m_stream.device())
            return m_stream.readRawData(s, len);

        NX_LOG("[media db] attempt to use null file stream", cl_logWARNING);
        return -1;
    }

private:
    QDataStream m_stream;
};

class DbHelper : public QnLongRunnable
{
    typedef std::deque<WriteRecordType> WriteQueue;
public:
    DbHelper(DbHelperHandler *const handler);
    ~DbHelper();

    DbHelper& operator = (DbHelper&& other) = default;

    Error readFileHeader(uint8_t *dbVersion);
    Error writeFileHeader(uint8_t dbVersion);

    Error readRecord();
    void writeRecord(const WriteRecordType &record);
    void stopWriter();
    void reset();

    void setMode(Mode mode);
    Mode getMode() const;

    QIODevice *getDevice() const;
    void setDevice(QIODevice *device);

public:
    virtual void run() override;
    virtual void stop() override;

private:
    Error getError() const;

private:
    QIODevice *m_device;
    DbHelperHandler *m_handler;
    WriteQueue m_writeQueue;
    FaultTolerantDataStream m_stream;

    mutable QnMutex m_mutex;
    QnWaitCondition m_cond;
    QnWaitCondition m_writerDoneCond;
    bool m_needStop;
    Mode m_mode;
};

} // namespace media_db
} // namespace nx

#endif // __MEDIA_DB_TYPES_H__