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
    NX_VERBOSE(this, lit("[CamInfo] writing camera info files starting..."));

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

    NX_VERBOSE(this, lit("[CamInfo] writing camera info files DONE"));
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
    NX_VERBOSE(this, lm("Writing camera info to %1. Data changed: %2").args(
        infoFilePath,
        isWriteNeeded(infoFilePath, infoFileData)));

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

    auto outFile = std::unique_ptr<QIODevice>(storage->open(path, QIODevice::WriteOnly | QIODevice::Truncate));
    if (!outFile)
    {
        NX_DEBUG(this, lit("%1. Create file failed for this path: %2")
                .arg(Q_FUNC_INFO)
                .arg(path));
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
    return m_camera->getUserDefinedGroupName();
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
            utils::log::Level::error
        };
        return false;
    }

    coreData.typeId = m_handler->archiveCamTypeId();
    if (coreData.typeId.isNull())
    {
        m_lastError = {
            lit("Unable to get type id for the archive camera %1")
                .arg(m_archiveCamData.coreData.physicalId),
            utils::log::Level::error
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
            utils::log::Level::verbose
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
            utils::log::Level::verbose
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
            utils::log::Level::error
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
        const auto result = parseLine(fileDataStream.readLine());
        if (result)
            addProperty(*result);
    }

    return true;
}

static int findMatchedDelimeter(const QString& src, char startBrace, char endBrace, bool include)
{
    int braceCount = 1;

    for (int i = 1; i < src.size(); ++i)
    {
        if (src[i] == startBrace && include)
            ++braceCount;
        else if (src[i] == endBrace)
            --braceCount;

        if (braceCount == 0)
        {
            if (include)
            {
                if (i == src.size() - 1)
                    return -1;
                return i + 1;
            }

            return i;
        }
    }

    return -1;
}

static bool readInnerData(const QString& src, char startBrace, char endBrace, QString* dst, bool include)
{
    const int endPos = findMatchedDelimeter(src, startBrace, endBrace, include);
    if (endPos == -1)
        return false;

    *dst = src.mid(0, endPos);
    return true;
}

static bool readValue(const QString& src, QString* dst)
{
    if (src[0] == '{')
        return readInnerData(src, '{', '}', dst, true);
    if (src[0] == '[')
        return readInnerData(src, '[', ']', dst, true);

    return readInnerData(src, '"', '"', dst, false);
}

boost::optional<Reader::ParseResult> Reader::parseLine(const QString& line) const
{
    if (line.isEmpty())
        return boost::none;

    enum
    {
        waitingForKey,
        readingKey,
        waitingForValue,
        readingValue,
        eqSign,
        done,
        failed
    } state = waitingForKey;

    QString key;
    QString value;
    int quoteCount = 0;
    bool finished = false;

    for (int i = 0; i < line.size(); ++i)
    {
        switch (state)
        {
            case waitingForKey:
                if (line[i] == '"')
                    state = readingKey;
                break;
            case readingKey:
                if (line[i] == '"')
                    state = eqSign;
                else
                    key.push_back(line[i]);
                break;
            case eqSign:
                if (line[i] != '=')
                    state = failed;
                else
                    state = waitingForValue;
                break;
            case waitingForValue:
                if (line[i] != '"')
                    state = failed;
                else
                    state = readingValue;
                break;
            case readingValue:
                if (readValue(line.mid(i), &value))
                    state = done;
                else
                    state = failed;
                break;
            case done:
            case failed:
                finished = true;
                break;
        }

        if (finished)
            break;
    }

    using ParseResult = Reader::ParseResult;
    return state == done ? boost::optional<ParseResult>(ParseResult(key, value)) : boost::none;
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
    {
        coreData.url = result.value();

        // TODO: #wearable This is a hack, and I'm failing to see an easy workaround.
        if (coreData.url.startsWith(lit("wearable://")))
            coreData.typeId = QnResourceTypePool::kWearableCameraTypeUuid;
    }
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
    NX_UTILS_LOG(errorInfo.severity, this, errorInfo.message);
}


} //namespace caminfo
} //namespace nx
