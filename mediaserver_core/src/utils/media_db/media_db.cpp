#include "media_db_types.h"

namespace nx
{
namespace media_db
{

namespace
{

QDataStream &operator << (QDataStream &stream, const FileHeader &fh)
{
    return stream << fh.byte_1 << fh.byte_2;
}

QDataStream &operator >> (QDataStream &stream, FileHeader &fh)
{
    return stream >> fh.byte_1 >> fh.byte_2;
}

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
    m_stream >> rb.byte_1;

    switch (rb.recordType())
    {
    case RecordType::FileOperationAdd:
        break;
    case RecordType::FileOperationDelete:
        break;
    case RecordType::CameraOperationAdd:
        break;
    }
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
                boost::apply_visitor(record);
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