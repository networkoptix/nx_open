#include "media_db.h"

namespace nx
{
namespace media_db
{

namespace
{

QDataStream &operator << (QDataStream &stream, const CameraOperation &cameraOp)
{
    return stream << cameraOp.part_1 << cameraOp.cameraUniqueId;
}

QDataStream &operator >> (QDataStream &stream, CameraOperation &cameraOp)
{
    return stream >> cameraOp.part_1 >> cameraOp.cameraUniqueId;
}

template<typename StructToWrite>
QDataStream &operator << (QDataStream &stream, const StructToWrite &s)
{
    return stream << s.part_1<< s.part_2;
}

template<typename StructToWrite>
QDataStream &operator >> (QDataStream &stream, StructToWrite &s)
{
    return stream >> s.part_1>> s.part_2;
}

class RecordVisitor : public boost::static_visitor<>
{
public:
    RecordVisitor(DbHelperHandler *handler, Error *error, QDataStream* stream)
        : m_handler(handler),
          m_error(error),
          m_stream(stream)
    {}
   
    template<typename StructToWrite>
    void operator()(const StructToWrite &s) const
    {
        *m_stream << s;
    }

private:
    DbHelperHandler *m_handler;
    Error *m_error;
    QDataStream *m_stream;
};

} // namespace <anonymous>

DbHelper::DbHelper(QIODevice *device, DbHelperHandler *const handler)
: m_device(device),
  m_handler(handler),
  m_stream(device),
  m_needStop(false)
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
    FileHeader fh;
    m_stream >> fh;
    if (version)
        *version = fh.getDbVersion();

    return getError();
}

Error DbHelper::writeFileHeader(uint8_t dbVersion)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    FileHeader fh;
    fh.setDbVersion(dbVersion);
    m_stream << fh;

    return getError();
}

Error DbHelper::readRecord()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    RecordBase rb;
    m_stream >> rb.part_1;
    quint64 part_2;
    Error error;

    switch (rb.recordType())
    {
    case RecordType::FileOperationAdd:
    case RecordType::FileOperationDelete:
    {
        m_stream >> part_2;
        error = getError();
        m_handler->handleMediaFileOp(MediaFileOperation(rb.part_1, part_2), error);
        break;
    }
    case RecordType::CameraOperationAdd:
    {
        CameraOperation cameraOp(rb.part_1);
        QByteArray bArray;
        bArray.resize(cameraOp.getCameraUniqueIdLen());
        m_stream.readRawData(bArray.data(), cameraOp.getCameraUniqueIdLen());
        cameraOp.cameraUniqueId = bArray;
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

void DbHelper::writeRecordAsync(const WriteRecordType &record)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_writeQueue.push(record);
}

void DbHelper::startWriter()
{
    m_thread = std::thread(
        [this]
        {
            while (1)
            {
                std::unique_lock<std::mutex> lk(m_mutex);
                m_cond.wait(lk, [this] { return !m_writeQueue.empty(); });
                
                if (m_needStop)
                    return;

                auto record = m_writeQueue.front();
                m_writeQueue.pop();

                Error error;
                RecordVisitor vis(m_handler, &error, &m_stream);
                boost::apply_visitor(vis, record);
                m_handler->handleRecordWrite(error);
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