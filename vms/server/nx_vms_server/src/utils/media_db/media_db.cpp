#include "media_db.h"

#include <QtCore/QTextStream>

#include <algorithm>
#include <cerrno>
#include <functional>

#include <recorder/device_file_catalog.h>
#include <nx/utils/log/log.h>
#include <nx/utils/url.h>

namespace nx::media_db {

namespace {

class RecordVisitor : public boost::static_visitor<>
{
public:
    RecordVisitor(QDataStream* stream, std::function<void(const QString&, int)> onError):
        m_stream(stream)
    {
    }

    void operator() (const CameraOperation& cameraOperation) const
    {
        ByteStreamWriter writer(cameraOperation.serializedRecordSize());
        cameraOperation.serialize(writer);

        m_stream->writeRawData(writer.data(), writer.data().size());
        if (m_stream->status() == QDataStream::WriteFailed)
            m_onError("camera", errno);
    }

    void operator() (const MediaFileOperation& mediaFileOperation) const
    {
        ByteStreamWriter writer(MediaFileOperation::kSerializedRecordSize);
        mediaFileOperation.serialize(writer);

        m_stream->writeRawData(writer.data(), writer.data().size());
        if (m_stream->status() == QDataStream::WriteFailed)
            m_onError("file", errno);
    }

    void operator() (const boost::blank&) const
    {
        NX_ASSERT(false, "Should never be here");
    }

private:
    QDataStream *m_stream;
    std::function<void(const QString&, int)> m_onError;
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

void CameraOperation::setCameraUniqueId(const QByteArray& uniqueId)
{
    part1 &= ~(getBitMask(0xe) << 0x2);
    part1 |= ((quint64)uniqueId.size() & getBitMask(0xe)) << 0x2;
    cameraUniqueId = uniqueId;
}

bool DbReader::deserialize(const QByteArray& buffer, Data* parsedData)
{
    ByteStreamReader reader(buffer);

    if (!parsedData->header.deserialize(&reader) || parsedData->header.getDbVersion() != kDbVersion)
        return false;

    std::set<int> knownCameraIds;

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
                if (knownCameraIds.find(operation.getCameraId()) == knownCameraIds.cend())
                    return false;

                int index = operation.getCameraId() * 2 + operation.getCatalog();

                if (operation.isClearWholeCatalogOperation())
                {
                    parsedData->removeRecords[index].clear();
                    parsedData->addRecords[index].clear();
                }
                else
                {
                    auto& container = parsedData->removeRecords[index];
                    container.emplace_back(DbReader::RemoveData{
                        operation.getHashInCatalog(), parsedData->addRecords[index].size()});
                }
                break;
            }
            case RecordType::FileOperationAdd:
            {
                if (!reader.hasBuffer(sizeof(quint64)))
                    return true;

                const auto operation = MediaFileOperation(rb.part1, reader.readUint64());
                if (knownCameraIds.find(operation.getCameraId()) == knownCameraIds.cend())
                    return false;

                int index = operation.getCameraId() * 2 + operation.getCatalog();
                auto& container = parsedData->addRecords[index];
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

                knownCameraIds.emplace(camera.getCameraId());
                parsedData->cameras.emplace_back(camera);
                break;
            }
            default:
                NX_ASSERT(false, "Should never be here");
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
    reportErrorIfNeeded();
    if (!m_stream.device())
        return;

    auto onErrorCb = [this](const QString& source, int err) { onError(source, err); };
    boost::apply_visitor(RecordVisitor(&m_stream, onErrorCb), record);
}

void MediaDbWriter::setDevice(QIODevice* ioDevice, const QString& fileName)
{
    m_stream.setDevice(ioDevice);
    m_fileName = fileName;
}

void MediaDbWriter::onError(const QString& source, int err)
{
    m_errnoCount[err] += 1;
    m_errorSourceCount[source] += 1;
}

void MediaDbWriter::reportErrorIfNeeded()
{
    const size_t errorTotal = std::accumulate(
        m_errnoCount.cbegin(), m_errnoCount.cend(), 0,
        [](size_t init, size_t val) { return init + val; });

    if (errorTotal != m_errorTotal
        && m_errorTimer.elapsed() > kReportErrorTresholdTime
        && nx::utils::log::isToBeLogged(nx::utils::log::Level::warning, this))
    {
        QString logMessage;
        QTextStream logStream(&logMessage);
        logStream << errorTotal << " media db write errors detected for file '"
            << nx::utils::url::hidePassword(m_fileName) << "'. ";

        for (auto it = m_errnoCount.begin(); it != m_errnoCount.end(); ++it)
            logStream << strerror(it.key()) << ": " << it.value() << ". ";

        for (auto it = m_errorSourceCount.begin(); it != m_errorSourceCount.end(); ++it)
            logStream << it.key() << ": " << it.value() << ". ";

        NX_WARNING(this, logMessage);
        m_errorTimer.restart();
        m_errorTotal = errorTotal;
    }
}

} // namespace nx::media_db
