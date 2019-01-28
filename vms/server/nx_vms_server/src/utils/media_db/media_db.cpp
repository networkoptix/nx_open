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

    void operator() (const CameraOperation& cameraOperation) const
    {
        ByteStreamWriter writer(cameraOperation.serializedRecordSize());
        cameraOperation.serialize(writer);

        m_stream->writeRawData(writer.data(), writer.data().size());
        if (m_stream->status() == QDataStream::WriteFailed)
        {
            qWarning()
                << "Media DB Camera operation write error: QDataStream::WriteRawData wrong bytes written count. errno: "
                << strerror(errno);
        }
    }

    void operator() (const MediaFileOperation& mediaFileOperation) const
    {
        ByteStreamWriter writer(MediaFileOperation::kSerializedRecordSize);
        mediaFileOperation.serialize(writer);

        m_stream->writeRawData(writer.data(), writer.data().size());
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

bool DbReader::deserialize(const QByteArray& buffer, Data* parsedData)
{
    ByteStreamReader reader(buffer);

    if (!parsedData->header.deserialize(&reader) || parsedData->header.getDbVersion() != kDbVersion)
        return false;

    while (reader.hasBuffer(sizeof(quint64)))
    {
        RecordBase rb(reader.readUint64());
        const auto recordType = rb.getRecordType();
        switch (recordType)
        {
            case RecordType::FileOperationDelete:
            {
                if (!reader.hasBuffer(sizeof(quint64)))
                    return true;

                const auto operation = MediaFileOperation(rb.part1, reader.readUint64());
                int index = operation.getCameraId() * 2 + operation.getCatalog();

                if (operation.isClearWholeCatalogOperation())
                {
                    parsedData->removeRecords[index].clear();
                    parsedData->addRecords[index].clear();
                }
                else
                {
                    auto& container = parsedData->removeRecords[index];
                    container.emplace_back(operation.getHashInCatalog());
                }
                break;
            }
            case RecordType::FileOperationAdd:
            {
                if (!reader.hasBuffer(sizeof(quint64)))
                    return true;

                const auto operation = MediaFileOperation(rb.part1, reader.readUint64());
                int index = operation.getCameraId() * 2 + operation.getCatalog();
                auto& container = parsedData->addRecords[index];
                if (container.empty() && parsedData->cameras.size() > 0)
                {
                    // Minor optimization. Try to forecast record count to reduce reallocate amount.
                    container.reserve(
                        buffer.size()
                        / MediaFileOperation::kSerializedRecordSize
                        / parsedData->cameras.size());
                }
                container.emplace_back(operation);
                break;
            }
            case RecordType::CameraOperationAdd:
            {
                CameraOperation camera(rb.part1);
                const int bytesToRead = camera.getCameraUniqueIdLen();
                camera.cameraUniqueId.resize(bytesToRead);
                int bytesRead = reader.readRawData(camera.cameraUniqueId.data(), bytesToRead);
                if (bytesRead != bytesToRead)
                    return true;

                parsedData->cameras.emplace_back(camera);
                break;
            }
            default:
                NX_ASSERT(false, "Shold never be here");
                return true;
        }
    }

    return true;
}

bool DbReader::parse(const QByteArray& fileContent, Data* parsedData)
{
    if (!deserialize(fileContent, parsedData))
        return false;

    for (auto& catalog : parsedData->removeRecords)
        std::sort(catalog.second.begin(), catalog.second.end());

    return true;
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
