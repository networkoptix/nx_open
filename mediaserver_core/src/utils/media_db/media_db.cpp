#include "media_db.h"

namespace nx
{
namespace media_db
{

namespace
{

QDataStream &operator << (QDataStream &stream, const CameraOperation &cameraOp)
{
    stream << cameraOp.part_1;
    stream.writeRawData(cameraOp.cameraUniqueId.data(), cameraOp.getCameraUniqueIdLen());

    return stream;
}

template<typename StructToWrite>
QDataStream &operator << (QDataStream &stream, const StructToWrite &s)
{
    return stream << s.part_1<< s.part_2;
}

class RecordVisitor : public boost::static_visitor<>
{
public:
    RecordVisitor(QDataStream* stream)
        : m_stream(stream)
    {}
   
    template<typename StructToWrite>
    void operator()(const StructToWrite &s) const
    {
        *m_stream << s;
    }

private:
    QDataStream *m_stream;
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

Error DbHelper::getError() const
{
    if (m_stream.atEnd())
        return Error::Eof;

    switch (m_stream.status())
    {
    case QDataStream::Status::ReadCorruptData:
    case QDataStream::Status::ReadPastEnd:
        return Error::ReadError;
    case QDataStream::Status::WriteFailed:
        return Error::WriteError;
    }

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
    Error error;

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
        cameraOp.cameraUniqueId.resize(cameraOp.getCameraUniqueIdLen());
        m_stream.readRawData(cameraOp.cameraUniqueId.data(),
                             cameraOp.getCameraUniqueIdLen());
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

                auto record = m_writeQueue.front();
                m_writeQueue.pop();

                RecordVisitor vis(&m_stream);
                boost::apply_visitor(vis, record);
                m_handler->handleRecordWrite(getError());

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