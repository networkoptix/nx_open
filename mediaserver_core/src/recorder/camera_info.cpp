#include <utils/common/util.h>
#include <utils/common/id.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <plugins/resource/archive_camera/archive_camera.h>
#include <recorder/device_file_catalog.h>
#include <recorder/storage_manager.h>
#include "camera_info.h"

namespace nx {
namespace caminfo {

const char* const kArchiveCameraUrlKey = "cameraUrl";
const char* const kArchiveCameraNameKey = "cameraName";
const char* const kArchiveCameraModelKey = "cameraModel";
const char* const kArchiveCameraGroupIdKey = "groupId";
const char* const kArchiveCameraGroupNameKey = "groupName";


QByteArray Composer::make(ComposerHandler* composerHandler)
{
    QByteArray result;
    if (!composerHandler)
        return result;

    m_stream.reset(new QTextStream(&result));
    m_handler = composerHandler;

    printKeyValue(kArchiveCameraNameKey, m_handler->name());
    printKeyValue(kArchiveCameraModelKey, m_handler->model());
    printKeyValue(kArchiveCameraGroupIdKey, m_handler->groupId());
    printKeyValue(kArchiveCameraGroupNameKey, m_handler->groupName());
    printKeyValue(kArchiveCameraUrlKey, m_handler->url());

    printProperties();

    return result;
}

QString Composer::formatString(const QString& source) const
{
    QString sourceCopy = source;
    QRegExp symbolsToRemove("\\n|\\r");
    sourceCopy.replace(symbolsToRemove, QString());
    return lit("\"") + sourceCopy + lit("\"");
}

void Composer::printKeyValue(const QString& key, const QString& value)
{
    *m_stream << formatString(key) << "=" << formatString(value) << endl;
}

void Composer::printProperties()
{
    for (const QPair<QString, QString> prop: m_handler->properties())
        printKeyValue(prop.first, prop.second);
}


Writer::Writer(WriterHandler* writeHandler):
    m_handler(writeHandler)
{}

void Writer::write()
{
    NX_LOG(lit("[CamInfo] writing camera info files starting..."), cl_logDEBUG2);

    for (auto& storageUrl: m_handler->storagesUrls())
        for (int i = 0; i < static_cast<int>(QnServer::ChunksCatalogCount); ++i)
            for (auto& cameraId: m_handler->camerasIds(static_cast<QnServer::ChunksCatalog>(i)))
            {
                if (m_handler->needStop())
                    return;
                writeInfoIfNeeded(
                    makeFullPath(storageUrl, static_cast<QnServer::ChunksCatalog>(i), cameraId),
                    m_composer.make(m_handler->composerHandler(cameraId)));
            }

    NX_LOG(lit("[CamInfo] writing camera info files DONE"), cl_logDEBUG2);
}

bool Writer::isWriteNeeded(const QString& infoFilePath, const QByteArray& infoFileData) const
{
    bool isDataAndPathValid = !infoFilePath.isEmpty() && !infoFileData.isEmpty();
    bool isDataChanged = !m_infoPathToCameraInfo.contains(infoFilePath) ||
                          m_infoPathToCameraInfo[infoFilePath] != infoFileData;

    return isDataAndPathValid && isDataChanged;
}

void Writer::writeInfoIfNeeded(const QString& infoFilePath, const QByteArray& infoFileData)
{
    NX_LOG(lit("%1: write camera info to %2. Data changed: %3")
            .arg(Q_FUNC_INFO)
            .arg(infoFilePath)
            .arg(isWriteNeeded(infoFilePath, infoFileData)), cl_logDEBUG2);

    if (isWriteNeeded(infoFilePath, infoFileData))
    {
        if (m_handler->handleFileData(infoFilePath, infoFileData))
            m_infoPathToCameraInfo[infoFilePath] = infoFileData;
    }
}

QString Writer::makeFullPath(
    const QString& storageUrl,
    QnServer::ChunksCatalog catalog,
    const QString& cameraId)
{
    auto separator = getPathSeparator(storageUrl);
    auto basePath = closeDirPath(storageUrl) + DeviceFileCatalog::prefixByCatalog(catalog) + separator;
    return basePath + cameraId + separator + lit("info.txt");
}

ServerWriterHandler::ServerWriterHandler(
    QnStorageManager* storageManager,
    QnResourcePool* resPool)
    :
    m_storageManager(storageManager),
    m_resPool(resPool)
{
}

QStringList ServerWriterHandler::storagesUrls() const
{
    QStringList result;
    for (const auto& storage: m_storageManager->getUsedWritableStorages())
        result.append(storage->getUrl());
    return result;
}

QStringList ServerWriterHandler::camerasIds(QnServer::ChunksCatalog catalog) const
{
    return m_storageManager->getAllCameraIdsUnderLock(catalog);
}

bool ServerWriterHandler::needStop() const
{
    return QnResource::isStopping();
}

bool ServerWriterHandler::handleFileData(const QString& path, const QByteArray& data)
{
    auto storage = m_storageManager->getStorageByUrlInternal(path);
    NX_ASSERT(storage);
    if (!storage)
        return false;

    auto outFile = std::unique_ptr<QIODevice>(storage->open(path, QIODevice::WriteOnly));
    if (!outFile)
    {
        NX_LOG(lit("%1. Create file failed for this path: %2")
                .arg(Q_FUNC_INFO)
                .arg(path), cl_logDEBUG1);
        return false;
    }
    outFile->write(data);
    return true;
}

QString ServerWriterHandler::name() const
{
    return m_camera->getUserDefinedName();
}

QString ServerWriterHandler::model() const
{
    return m_camera->getModel();
}

QString ServerWriterHandler::groupId() const
{
    return m_camera->getGroupId();
}

QString ServerWriterHandler::groupName() const
{
    return m_camera->getGroupName();
}

QString ServerWriterHandler::url() const
{
    return m_camera->getUrl();
}

QList<QPair<QString, QString>> ServerWriterHandler::properties() const
{
    QList<QPair<QString, QString>> result;

    for (const auto& prop: m_camera->getAllProperties())
        result.append(QPair<QString, QString>(prop.name, prop.value));

    return result;
}

ComposerHandler* ServerWriterHandler::composerHandler(const QString& cameraId)
{
    m_camera = m_resPool->getResourceByUniqueId<QnSecurityCamResource>(cameraId);
    if (!static_cast<bool>(m_camera))
        return nullptr;
    return this;
}


Reader::Reader(ReaderHandler* readerHandler,
               const QnAbstractStorageResource::FileInfo& fileInfo,
               std::function<QByteArray(const QString&)> getFileDataFunc):
    m_handler(readerHandler),
    m_fileInfo(&fileInfo),
    m_getDataFunc(getFileDataFunc)
{}

void Reader::operator()(ArchiveCameraDataList* outArchiveCameraList)
{
    if (!initArchiveCamData()
        || cameraAlreadyExists(outArchiveCameraList)
        || !readFileData()
        || !parseData())
    {
        m_handler->handleError(m_lastError);
        return;
    }

    outArchiveCameraList->push_back(m_archiveCamData);
}

bool Reader::initArchiveCamData()
{
    ec2::ApiCameraData& coreData = m_archiveCamData.coreData;

    coreData.physicalId = m_fileInfo->fileName();
    if (coreData.physicalId.isEmpty())
        coreData.id = QnUuid();
    else
        coreData.id = guidFromArbitraryData(m_archiveCamData.coreData.physicalId.toUtf8());

    coreData.parentId = m_handler->moduleGuid();
    if (coreData.parentId.isNull())
    {
        m_lastError = {
            lit("Unable to get parent id for the archive camera %1")
                .arg(m_archiveCamData.coreData.physicalId),
            cl_logERROR
        };
        return false;
    }

    coreData.typeId = m_handler->archiveCamTypeId();
    if (coreData.typeId.isNull())
    {
        m_lastError = {
            lit("Unable to get type id for the archive camera %1")
                .arg(m_archiveCamData.coreData.physicalId),
            cl_logERROR
        };
        return false;
    }

    return true;
}

bool Reader::cameraAlreadyExists(const ArchiveCameraDataList* cameraList) const
{
    if (m_handler->isCameraInResPool(m_archiveCamData.coreData.id))
    {
        m_lastError = {
            lit("Archive camera %1 found but we already have camera with this id in the resource pool. Skipping.")
                .arg(m_archiveCamData.coreData.physicalId),
            cl_logDEBUG2
        };
        return true;
    }

    if(std::find_if(
                cameraList->cbegin(),
                cameraList->cend(),
                [this](const ArchiveCameraData& cam)
                {
                    return cam.coreData.id == m_archiveCamData.coreData.id;
                }) != cameraList->cend())
    {
        m_lastError = {
            lit("Camera %1 is already in the archive camera list")
                .arg(m_archiveCamData.coreData.physicalId),
            cl_logDEBUG2
        };
        return true;
    }

    return false;
}

QString Reader::infoFilePath() const
{
    return closeDirPath(m_fileInfo->absoluteFilePath()) + lit("info.txt");
}

bool Reader::readFileData()
{
    m_fileData = m_getDataFunc(infoFilePath());

    if (m_fileData.isNull())
    {
        m_lastError =  {
            lit("File data is NULL for archive camera %1")
                .arg(m_archiveCamData.coreData.physicalId),
            cl_logERROR
        };
        return false;
    }

    return true;
}

bool Reader::parseData()
{
    QTextStream fileDataStream(&m_fileData);
    while (!fileDataStream.atEnd())
    {
        ParseResult result = parseLine(fileDataStream.readLine());
        switch (result.code())
        {
        case ParseResult::ParseCode::NoData: break;
        case ParseResult::ParseCode::RegexpFailed:
            m_lastError = {
                lit("Camera info file %1 parse failed")
                    .arg(infoFilePath()),
                cl_logERROR
            };
            return false;
        case ParseResult::ParseCode::Ok:
            addProperty(result);
            break;
        }
    }

    return true;
}

Reader::ParseResult Reader::parseLine(const QString& line) const
{
    if (line.isEmpty())
        return ParseResult::ParseCode::NoData;

    thread_local QRegExp keyValueRegExp("^\"(.*)\"=\"(.*)\"$");

    int reIndex = keyValueRegExp.indexIn(line);
    if (reIndex == -1 || keyValueRegExp.captureCount() != 2)
        return ParseResult::ParseCode::RegexpFailed;

    auto captureList = keyValueRegExp.capturedTexts();
    return ParseResult(ParseResult::ParseCode::Ok, captureList[1], captureList[2]);
}

void Reader::addProperty(const ParseResult& result)
{
    ec2::ApiCameraData& coreData = m_archiveCamData.coreData;

    if (result.key().contains(kArchiveCameraNameKey))
        coreData.name = result.value();
    else if (result.key().contains(kArchiveCameraModelKey))
        coreData.model = result.value();
    else if (result.key().contains(kArchiveCameraGroupIdKey))
        coreData.groupId= result.value();
    else if (result.key().contains(kArchiveCameraGroupNameKey))
        coreData.groupName = result.value();
    else if (result.key().contains(kArchiveCameraUrlKey))
        coreData.url = result.value();
    else
        m_archiveCamData.properties.emplace_back(result.key(), result.value());
}

ServerReaderHandler::ServerReaderHandler(const QnUuid& moduleId, QnResourcePool* resPool):
    m_moduleId(moduleId),
    m_resPool(resPool)
{
}

QnUuid ServerReaderHandler::moduleGuid() const
{
    return m_moduleId;
}

QnUuid ServerReaderHandler::archiveCamTypeId() const
{
    if (m_archiveCamTypeId.isNull())
        m_archiveCamTypeId = qnResTypePool->getLikeResourceTypeId("", QnArchiveCamResource::cameraName());
    return m_archiveCamTypeId;
}

bool ServerReaderHandler::isCameraInResPool(const QnUuid& cameraId) const
{
    return static_cast<bool>(m_resPool->getResourceById(cameraId));
}

void ServerReaderHandler::handleError(const ReaderErrorInfo& errorInfo) const
{
    NX_LOG(errorInfo.message, errorInfo.severity);
}


} //namespace caminfo
} //namespace nx
