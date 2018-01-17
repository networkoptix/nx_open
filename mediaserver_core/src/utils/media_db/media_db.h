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
    RecordType getRecordType() const;
    void setRecordType(RecordType recordType);
};

struct MediaFileOperation : RecordBase
{
    quint64 part2;

    MediaFileOperation(quint64 i1 = 0, quint64 i2 = 0) : RecordBase(i1), part2(i2) {}
    int getCameraId() const;
    void setCameraId(int cameraId);

    qint64 getStartTime() const;
    void setStartTime(qint64 startTime);

    int getDuration() const;
    void setDuration(int duration);

    int getTimeZone() const;
    void setTimeZone(int timeZone);

    qint64 getFileSize() const;
    void setFileSize(qint64 fileSize);

    int getFileTypeIndex() const;
    void setFileTypeIndex(int fileTypeIndex);

    int getCatalog() const;
    void setCatalog(int catalog);
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