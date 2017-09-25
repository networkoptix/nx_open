#include <algorithm>
#include "media_db.h"
#include <cerrno>

#include <recorder/device_file_catalog.h>

namespace nx
{
namespace media_db
{

const size_t kMaxWriteQueueSize = 1000;

namespace
{

template<typename StructToWrite>
QDataStream &operator << (QDataStream &stream, const StructToWrite &s)
{
    return stream << s.part1 << s.part2;
}

class RecordVisitor : public boost::static_visitor<>
{
public:
    RecordVisitor(FaultTolerantDataStream* stream, Error *error)
        : m_stream(stream),
          m_error(error)
    {}

    void operator() (const CameraOperation &camOp) const
    {
        *m_stream << camOp.part1;
        if (m_stream->status() == QDataStream::WriteFailed)
        {
            qWarning() << "Media DB Camera operation write error: QDataStream::WriteFailed. errno: "
                       << strerror(errno);
            *m_error = Error::WriteError;
            return;
        }
        int bytesToWrite = camOp.getCameraUniqueIdLen();
        int bytesWritten = m_stream->writeRawData(camOp.cameraUniqueId.data(),
                                                  bytesToWrite);
        if (bytesToWrite != bytesWritten)
        {
            qWarning() << "Media DB Camera operation write error: QDataStream::WriteRawData wrong bytes written count. errno: "
                       << strerror(errno)
                       << ". Bytes to write = " << bytesToWrite
                       << "; Bytes written = " << bytesWritten;
            *m_error = Error::WriteError;
        }
    }

    template<typename StructToWrite>
    void operator() (const StructToWrite &s) const
    {
        *m_stream << s;
        if (m_stream->status() == QDataStream::WriteFailed)
        {
            qWarning() << "Media DB Media File operation write error: QDataStream::WriteFailed. errno: "
                       << strerror(errno);
            *m_error = Error::WriteError;
            return;
        }
    }

private:
    FaultTolerantDataStream *m_stream;
    Error *m_error;
};

} // namespace <anonymous>


int MediaFileOperation::getFileTypeIndex() const
{
    return ((part2 >> 0x3e) & 0x1) == 0
        ? DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION
        : DeviceFileCatalog::Chunk::FILE_INDEX_NONE;
}

void MediaFileOperation::setFileTypeIndex(int fileTypeIndex)
{
    NX_ASSERT(
        fileTypeIndex == DeviceFileCatalog::Chunk::FILE_INDEX_NONE
        || fileTypeIndex == DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION);

    quint64 value = fileTypeIndex == DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION ? 0x0LL : 0x1LL;
    part2 |= (value & 0x1) << 0x3e;
}


DbHelper::DbHelper(DbHelperHandler *const handler)
: m_device(nullptr),
  m_handler(handler),
  m_stream(m_device),
  m_needStop(false),
  m_mode(Mode::Read)
{
    m_stream.setByteOrder(QDataStream::LittleEndian);
    start();
}

void DbHelper::run()
{
    while (!m_needStop)
    {
        QnMutexLocker lk(&m_mutex);
        while ((m_writeQueue.empty() || m_mode == Mode::Read) && !m_needStop)
        {
            NX_LOG(lit("media_db: run is going to sleep. Write queue size: %1. Mode = %2. Need to stop = %3")
                    .arg(m_writeQueue.size())
                    .arg(m_mode == Mode::Read ? "Read" : m_mode == Mode::Write ? "Write" : "Unknown")
                    .arg(m_needStop), cl_logDEBUG2);
            m_cond.wait(lk.mutex());
        }

        if (m_needStop)
            return;

        auto record = m_writeQueue.front();

        Error error = Error::NoError;
        RecordVisitor vis(&m_stream, &error);

        lk.unlock();
        boost::apply_visitor(vis, record);
        lk.relock();

        m_writeQueue.pop_front();

        if (error == Error::WriteError)
        {
            m_handler->handleRecordWrite(error);
            continue;
        }

        error = getError();
        if (error == Error::WriteError)
        {
            m_handler->handleRecordWrite(error);
            continue;
        }
        else
            m_handler->handleRecordWrite(error);

        m_writerDoneCond.wakeAll();
    }
}

void DbHelper::stop()
{
    {
        QnMutexLocker lk(&m_mutex);
        m_needStop = true;
    }
    m_cond.wakeAll();

    wait();
}

DbHelper::~DbHelper()
{
    stop();
}

QIODevice *DbHelper::getDevice() const
{
    QnMutexLocker lk(&m_mutex);
    return m_device;
}

void DbHelper::setDevice(QIODevice *device)
{
    QnMutexLocker lk(&m_mutex);
    m_device = device;
    m_stream.setDevice(m_device);
    m_stream.resetStatus();
}

Error DbHelper::getError() const
{
    switch (m_stream.status())
    {
    case QDataStream::Status::ReadCorruptData:
    case QDataStream::Status::ReadPastEnd:
        return Error::ReadError;
    case QDataStream::Status::WriteFailed:
        return Error::WriteError;
    }

    if (m_stream.atEnd())
        return Error::Eof;

    return Error::NoError;
}

Error DbHelper::readFileHeader(uint8_t *version)
{
    QnMutexLocker lk(&m_mutex);
    if (m_mode == Mode::Write)
        return Error::WrongMode;
    FileHeader fh;
    m_stream >> fh.part1 >> fh.part2;
    if (version)
        *version = fh.getDbVersion();

    return getError();
}

Error DbHelper::writeFileHeader(uint8_t dbVersion)
{
    QnMutexLocker lk(&m_mutex);
    if (m_mode == Mode::Read)
        return Error::WrongMode;
    FileHeader fh;
    fh.setDbVersion(dbVersion);
    m_stream << fh;

    return getError();
}

Error DbHelper::readRecord()
{
    QnMutexLocker lk(&m_mutex);
    if (m_mode == Mode::Write)
        return Error::WrongMode;
    RecordBase rb;
    m_stream >> rb.part1;

    Error error = getError();
    if (error != Error::NoError)
    {
        m_handler->handleError(Error::ReadError);
        return error;
    }

    switch (rb.getRecordType())
    {
    case RecordType::FileOperationAdd:
    case RecordType::FileOperationDelete:
    {
        quint64 part2;
        m_stream >> part2;
        error = getError();
        m_handler->handleMediaFileOp(MediaFileOperation(rb.part1, part2), error);
        break;
    }
    case RecordType::CameraOperationAdd:
    {
        CameraOperation cameraOp(rb.part1);
        int bytesToRead = cameraOp.getCameraUniqueIdLen();
        cameraOp.cameraUniqueId.resize(bytesToRead);
        int bytesRead = m_stream.readRawData(cameraOp.cameraUniqueId.data(), bytesToRead);
        if (bytesRead != bytesToRead)
            error = Error::ReadError;
        else
            error = getError();
        m_handler->handleCameraOp(cameraOp, error);
        break;
    }
    default:
        m_handler->handleError(Error::ParseError);
        error = Error::ParseError;
        break;
    }

    return error;
}

void DbHelper::writeRecord(const WriteRecordType &record)
{
    QnMutexLocker lk(&m_mutex);
    while (m_writeQueue.size() > kMaxWriteQueueSize)
    {
        NX_LOG(lit("media_db: Write queue overflow detected. WriteRecord is going to sleep. Write queue size: %1.")
                .arg(m_writeQueue.size()), cl_logDEBUG2);
        m_writerDoneCond.wait(lk.mutex());
    }

    m_writeQueue.push_back(record);
    m_cond.wakeAll();
}

void DbHelper::reset()
{
    QnMutexLocker lk(&m_mutex);
    m_stream.resetStatus();
}

void DbHelper::setMode(Mode mode)
{
    QnMutexLocker lk(&m_mutex);
    if (mode == m_mode)
        return;

    while (!m_writeQueue.empty() && mode == Mode::Read)
    {
        NX_LOG(lit("media_db: SetMode is going to sleep. Write queue size: %1. Mode = %2")
                .arg(m_writeQueue.size())
                .arg(m_mode == Mode::Read ? "Read" : m_mode == Mode::Write ? "Write" : "Unknown"), cl_logDEBUG2);
        m_writerDoneCond.wait(lk.mutex());
    }

    m_stream.resetStatus();
    m_mode = mode;

    m_cond.wakeAll();
}

Mode DbHelper::getMode() const
{
    QnMutexLocker lk(&m_mutex);
    return m_mode;
}

} // namespace media_db
} // namespace nx
