#include <algorithm>
#include "media_db.h"
#include <nx/utils/log/log.h>

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
    RecordVisitor(QDataStream* stream, Error *error, QnMutex *mutex) 
        : m_stream(stream),
          m_error(error),
          m_mutex(mutex)
    {}
   
    void operator() (const CameraOperation &camOp) const
    {
        QnMutexLocker lk(m_mutex);
        *m_stream << camOp.part1;
        if (m_stream->status() == QDataStream::WriteFailed)
        {
            *m_error = Error::WriteError;
            return;
        }
        int bytesToWrite = camOp.getCameraUniqueIdLen();
        int bytesWritten = m_stream->writeRawData(camOp.cameraUniqueId.data(),
                                                  bytesToWrite);
        if (bytesToWrite != bytesWritten)
            *m_error = Error::WriteError;
    }

    template<typename StructToWrite>
    void operator() (const StructToWrite &s) const 
    { 
        QnMutexLocker lk(m_mutex);
        *m_stream << s; 
    }

private:
    QDataStream *m_stream;
    Error *m_error;
    QnMutex *m_mutex;
};

} // namespace <anonymous>

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
    while (1)
    {
        QnMutexLocker lk(&m_writeQueueMutex);
        while ((m_writeQueue.empty() || m_mode == Mode::Read) && !m_needStop)
            m_cond.wait(lk.mutex());
        
        if (m_needStop)
            return;

        while (!m_writeQueue.empty())
        {
            auto record = m_writeQueue.front();

            Error error;
            RecordVisitor vis(&m_stream, &error, &m_mutex);
            lk.unlock();
            boost::apply_visitor(vis, record);
            lk.relock();
            m_writeQueue.pop_front();

            QnMutexLocker lk2(&m_mutex);
            if (error == Error::WriteError)
            {
                m_handler->handleRecordWrite(error);
                break;
            }

            error = getError();
            if (error == Error::WriteError)
            {
                m_handler->handleRecordWrite(error);
                break;
            } 
            else
                m_handler->handleRecordWrite(error);
        }

        lk.unlock();
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
    {
        QnMutexLocker lk(&m_writeQueueMutex);
        if (m_writeQueue.size() > kMaxWriteQueueSize)
            NX_LOG(lit("%1 DB write queue overflowed. Waiting while it becomes empty...").arg(Q_FUNC_INFO), cl_logWARNING);
        while (m_writeQueue.size() > kMaxWriteQueueSize)
            m_writerDoneCond.wait(lk.mutex());
        m_writeQueue.push_back(record);
    }
    m_cond.wakeAll();
}

void DbHelper::reset()
{
    QnMutexLocker lk(&m_mutex);
    m_stream.resetStatus();
}

void DbHelper::setMode(Mode mode)
{
    {
        QnMutexLocker lk(&m_mutex);
        if (mode == m_mode)
            return;
    }

    {
        QnMutexLocker lk(&m_writeQueueMutex);
        while (!m_writeQueue.empty() && mode == Mode::Read)
            m_writerDoneCond.wait(lk.mutex());
    }

    {
        QnMutexLocker lk(&m_mutex);
        m_stream.resetStatus();
        m_mode = mode;
    }
    m_cond.wakeAll();
}

Mode DbHelper::getMode() const
{
    QnMutexLocker lk(&m_mutex);
    return m_mode;
}

} // namespace media_db
} // namespace nx
