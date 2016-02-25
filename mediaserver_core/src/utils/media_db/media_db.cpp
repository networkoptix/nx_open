#include "media_db.h"

namespace nx
{
namespace media_db
{

namespace
{

template<typename StructToWrite>
QDataStream &operator << (QDataStream &stream, const StructToWrite &s)
{
    return stream << s.part_1 << s.part_2;
}

class RecordVisitor : public boost::static_visitor<>
{
public:
    RecordVisitor(QDataStream* stream, Error *error) : m_stream(stream), m_error(error) {}
   
    void operator() (const CameraOperation &camOp) const
    {
        *m_stream << camOp.part_1;
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
    void operator() (const StructToWrite &s) const { *m_stream << s; }

private:
    QDataStream *m_stream;
    Error *m_error;
};

} // namespace <anonymous>

DbHelper::DbHelper(QIODevice *device, DbHelperHandler *const handler)
: m_device(device),
  m_handler(handler),
  m_stream(device),
  m_needStop(false),
  m_mode(Mode::Read)
{
    m_stream.setByteOrder(QDataStream::LittleEndian);
    startWriter();
}

DbHelper::~DbHelper()
{
    stopWriter();
}

QIODevice *DbHelper::getDevice() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_device;
}

void DbHelper::setDevice(QIODevice *device)
{
    std::lock_guard<std::mutex> lk(m_mutex);
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
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_mode == Mode::Write)
        return Error::WrongMode;
    FileHeader fh;
    m_stream >> fh.part_1 >> fh.part_2;
    if (version)
        *version = fh.getDbVersion();

    return getError();
}

Error DbHelper::writeFileHeader(uint8_t dbVersion)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_mode == Mode::Read)
        return Error::WrongMode;
    FileHeader fh;
    fh.setDbVersion(dbVersion);
    m_stream << fh;

    return getError();
}

Error DbHelper::readRecord()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_mode == Mode::Write)
        return Error::WrongMode;
    RecordBase rb;
    m_stream >> rb.part_1;

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
        quint64 part_2;
        m_stream >> part_2;
        error = getError();
        m_handler->handleMediaFileOp(MediaFileOperation(rb.part_1, part_2), error);
        break;
    }
    case RecordType::CameraOperationAdd:
    {
        CameraOperation cameraOp(rb.part_1);
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

Error DbHelper::writeRecordAsync(const WriteRecordType &record)
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_mode == Mode::Read)
            return Error::WrongMode;
        m_writeQueue.push(record);
    }
    m_cond.notify_all();
    return Error::NoError;
}

void DbHelper::reset()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_stream.resetStatus();
}

void DbHelper::setMode(Mode mode)
{
    std::unique_lock<std::mutex> lk(m_mutex);
    m_mode = mode;
    m_stream.resetStatus();
    if (!m_writeQueue.empty())
        m_writerDoneCond.wait(lk, [this] { return m_writeQueue.empty(); });
}

Mode DbHelper::getMode() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_mode;
}

void DbHelper::startWriter()
{
    m_thread = std::thread(
        [this]
        {
            while (1)
            {
                std::unique_lock<std::mutex> lk(m_mutex);
                m_cond.wait(lk, [this] { return !m_writeQueue.empty() || m_needStop; });
                
                if (m_needStop)
                    return;

                while (!m_writeQueue.empty())
                {
                    auto record = m_writeQueue.front();
                    m_writeQueue.pop();

                    Error error;
                    RecordVisitor vis(&m_stream, &error);
                    boost::apply_visitor(vis, record);
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
                m_writerDoneCond.notify_all();
            }
        });
}

void DbHelper::stopWriter()
{
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_needStop = true;
    }
    m_cond.notify_all();

    if (m_thread.joinable())
        m_thread.join();
}

} // namespace media_db
} // namespace nx