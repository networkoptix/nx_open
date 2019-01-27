#include <algorithm>
#include "media_db.h"
#include <cerrno>

#include <recorder/device_file_catalog.h>

namespace nx::media_db {

namespace {

class RecordVisitor : public boost::static_visitor<>
{
public:
    RecordVisitor(QDataStream* stream): m_stream(stream) {}

    void operator() (const CameraOperation &camOp) const
    {
        *m_stream << camOp.part1;
        if (m_stream->status() == QDataStream::WriteFailed)
        {
            qWarning()
                << "Media DB Camera operation write error: QDataStream::WriteFailed. errno:"
                << strerror(errno);
            return;
        }
        int bytesToWrite = camOp.getCameraUniqueIdLen();
        int bytesWritten = m_stream->writeRawData(camOp.cameraUniqueId.data(),
                                                  bytesToWrite);
        if (bytesToWrite != bytesWritten)
        {
            qWarning()
                << "Media DB Camera operation write error: QDataStream::WriteRawData wrong bytes written count. errno: "
                << strerror(errno)
                << ". Bytes to write = " << bytesToWrite
                << "; Bytes written = " << bytesWritten;
        }
    }

    void operator() (const MediaFileOperation &s) const
    {
        QByteArray tmpBuffer;
        tmpBuffer.resize(sizeof(s.part1) + sizeof(s.part2));

        decltype (s.part1) *part1 = (decltype (s.part1)*)tmpBuffer.data();
        decltype (s.part2) *part2 = (decltype (s.part2)*)(tmpBuffer.data() + sizeof(s.part1));
        *part1 = qToLittleEndian(s.part1);
        *part2 = qToLittleEndian(s.part2);

        m_stream->writeRawData(tmpBuffer.data(), tmpBuffer.size());

        if (m_stream->status() == QDataStream::WriteFailed)
        {
            qWarning()
                << "Media DB Media File operation write error: QDataStream::WriteFailed. errno: "
                << strerror(errno);
            return;
        }
    }

    void operator() (const boost::blank&) const
    {
        NX_ASSERT(false, "Should never be here");
    }

private:
    QDataStream *m_stream;
};


bool streamFailed(const QDataStream& stream)
{
    switch (stream.status())
    {
        case QDataStream::Status::ReadCorruptData:
        case QDataStream::Status::ReadPastEnd:
        case QDataStream::Status::WriteFailed:
            return true;
        case QDataStream::Status::Ok:
            break;
    }

    return false;
}

} // namespace <anonymous>

int CameraOperation::getCameraUniqueIdLen() const
{
    return (part1 >> 0x2) & getBitMask(0xE);
}

void CameraOperation::setCameraUniqueIdLen(int len)
{
    part1 &= ~(getBitMask(0xe) << 0x2);
    part1 |= ((quint64)len & getBitMask(0xe)) << 0x2;
}

int CameraOperation::getCameraId() const
{
    return (part1 >> 0x10) & getBitMask(0x10);
}

void CameraOperation::setCameraId(int cameraId)
{
    part1 &= ~(getBitMask(0x10) << 0x10);
    part1 |= ((quint64)cameraId & getBitMask(0x10)) << 0x10;
}

QByteArray CameraOperation::getCameraUniqueId() const
{
    return cameraUniqueId;
}

void CameraOperation::setCameraUniqueId(const QByteArray &uniqueId)
{
    cameraUniqueId = uniqueId;
}

MediaDbReader::MediaDbReader(QIODevice* ioDevice)
{
    m_stream.setDevice(ioDevice);
    m_stream.setByteOrder(QDataStream::LittleEndian);
}

bool MediaDbReader::readFileHeader(uint8_t *dbVersion)
{
    FileHeader fh;
    m_stream >> fh.part1 >> fh.part2;
    if (dbVersion)
        *dbVersion = fh.getDbVersion();

    return !streamFailed(m_stream);
}

DBRecord MediaDbReader::readRecord()
{
    RecordBase rb;
    m_stream >> rb.part1;

    if (streamFailed(m_stream) || m_stream.atEnd())
        return DBRecord();

    switch (rb.getRecordType())
    {
        case RecordType::FileOperationAdd:
        case RecordType::FileOperationDelete:
        {
            quint64 part2;
            m_stream >> part2;
            if (streamFailed(m_stream))
                return DBRecord();

            return MediaFileOperation(rb.part1, part2);
        }
        case RecordType::CameraOperationAdd:
        {
            CameraOperation cameraOp(rb.part1);
            int bytesToRead = cameraOp.getCameraUniqueIdLen();
            cameraOp.cameraUniqueId.resize(bytesToRead);
            int bytesRead = m_stream.readRawData(cameraOp.cameraUniqueId.data(), bytesToRead);
            if (bytesRead != bytesToRead)
                return DBRecord();

            return cameraOp;
        }
        default:
            break;
    }

    return DBRecord();
}

MediaDbWriter::MediaDbWriter()
{
    m_stream.setByteOrder(QDataStream::LittleEndian);
}

MediaDbWriter::~MediaDbWriter()
{
    stop();
}

void MediaDbWriter::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    {
        QnMutexLocker lock(&m_mutex);
        m_waitCondition.wakeOne();
    }
}

bool MediaDbWriter::writeFileHeader(QIODevice* ioDevice, uint8_t dbVersion)
{
    FileHeader fh;
    fh.setDbVersion(dbVersion);
    QDataStream stream(ioDevice);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << fh.part1 << fh.part2;
    return !streamFailed(stream);
}

void MediaDbWriter::writeRecord(const DBRecord &record)
{
    QnMutexLocker lock(&m_mutex);
    m_queue.push(record);
    m_waitCondition.wakeOne();
}

void MediaDbWriter::run()
{
    QnMutexLocker lock(&m_mutex);
    while (!m_needStop)
    {
        while (m_queue.empty() && !needToStop())
            m_waitCondition.wait(&m_mutex);

        while (!m_queue.empty())
        {
            auto record = m_queue.front();
            m_queue.pop();
            lock.unlock();
            boost::apply_visitor(RecordVisitor(&m_stream), record);
            lock.relock();
        }
    }
}

void MediaDbWriter::setDevice(QIODevice* ioDevice)
{
    m_stream.setDevice(ioDevice);
}

} // namespace nx::media_db
