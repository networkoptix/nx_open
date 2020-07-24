#include "camera_info.h"

#include <utils/common/util.h>
#include <utils/common/id.h>
#include <utils/crypt/symmetrical.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <plugins/resource/archive_camera/archive_camera.h>
#include <recorder/device_file_catalog.h>
#include <recorder/storage_manager.h>

namespace nx {
namespace caminfo {

const char* const kArchiveCameraUrlKey = "cameraUrl";
const char* const kArchiveCameraNameKey = "cameraName";
const char* const kArchiveCameraModelKey = "cameraModel";
const char* const kArchiveCameraGroupIdKey = "groupId";
const char* const kArchiveCameraGroupNameKey = "groupName";

static std::map<QString, QString> gatherProps(const QnSecurityCamResourcePtr& camera)
{
    std::map<QString, QString> result;
    result.insert({kArchiveCameraNameKey, camera->getUserDefinedName()});
    result.insert({kArchiveCameraModelKey, camera->getModel()});
    result.insert({kArchiveCameraGroupIdKey, camera->getGroupId()});
    result.insert({kArchiveCameraGroupNameKey, camera->getUserDefinedGroupName()});
    result.insert({kArchiveCameraUrlKey, camera->getUrl()});

    for (const auto& prop: camera->getAllProperties())
        result.insert({prop.name, prop.value});

    return result;
}

QByteArray infoFrom(const QnSecurityCamResourcePtr& camera)
{
    QByteArray result;
    QTextStream stream(&result);
    stream.setCodec(QTextCodec::codecForName("UTF-8"));
    stream << "VERSION 1.1" << endl;
    for (const auto& p: gatherProps(camera))
    {
        const auto key = nx::utils::encodeHexStringFromStringAES128CBC(p.first);
        const auto value = nx::utils::encodeHexStringFromStringAES128CBC(p.second);
        stream << key << "=" << value << endl;
    }

    return result;
}

static IdToInfo fromCameras(const QnVirtualCameraResourceList& cameras)
{
    IdToInfo result;
    for (const auto& camera: cameras)
        result.insert({camera->getUniqueId(), infoFrom(camera)});

    return result;
}

static std::vector<std::pair<QString, StorageResourcePtr>> genPathsForQuality(
    const StorageResourceList& storages,
    QnServer::ChunksCatalog quality)
{
    std::vector<std::pair<QString, StorageResourcePtr>> result;
    for (const auto& storage: storages)
    {
        const auto qualityPostfix = DeviceFileCatalog::prefixByCatalog(quality);
        result.push_back({closeDirPath(storage->getUrl()) + qualityPostfix, storage});
    }

    return result;
}

static std::vector<std::pair<QString, StorageResourcePtr>> genPaths(
    const StorageResourceList& storages)
{
    auto hiQualityPaths = genPathsForQuality(storages, QnServer::HiQualityCatalog);
    const auto lowQualityPaths = genPathsForQuality(storages, QnServer::LowQualityCatalog);
    std::vector<std::pair<QString, StorageResourcePtr>> result;

    result.reserve(hiQualityPaths.size() + lowQualityPaths.size());
    for (auto& p: hiQualityPaths)
        result.emplace_back(std::move(p));

    for (auto& p: lowQualityPaths)
        result.emplace_back(std::move(p));

    return result;
}

static void writeFile(
    const std::pair<QString, QByteArray>& idToInfo,
    const std::vector<std::pair<QString, StorageResourcePtr>>& pathsWithStorages)
{
    for (const auto& pathWithStorage: pathsWithStorages)
    {
        const auto filePath = closeDirPath(pathWithStorage.first) + idToInfo.first + "/info.txt";
        auto file = std::unique_ptr<QIODevice>(pathWithStorage.second->open(
           filePath,
           QIODevice::WriteOnly | QIODevice::Truncate));

        if (!file)
        {
            NX_DEBUG(typeid(Writer), "Failed to open file '%1'", filePath);
            continue;
        }

        if (file->write(idToInfo.second))
            NX_DEBUG(typeid(Writer), "Successfully written file '%1'", filePath);
        else
            NX_DEBUG(typeid(Writer), "Failed to write file '%1'", filePath);
    }
}

static std::unordered_set<QString> toUniquePaths(
    const std::vector<std::pair<QString, StorageResourcePtr>>& pathStoragePairs)
{
    std::unordered_set<QString> result;
    for (const auto& p: pathStoragePairs)
        result.insert(p.first);

    return result;
}

Writer::Writer(const QnMediaServerModule* serverModule): m_serverModule(serverModule)
{
}

QnVirtualCameraResourceList Writer::getCameras(const QStringList& cameraIds) const
{
    const auto resPool = m_serverModule->resourcePool();
    QnVirtualCameraResourceList result;
    for (const auto& cameraId: cameraIds)
    {
        const auto camera = resPool->getResourceByUniqueId(cameraId).dynamicCast<QnVirtualCameraResource>();
        if (camera)
            result.push_back(camera);
    }

    return result;
}

void Writer::writeAll(const StorageResourceList& storages, const QStringList& cameraIds)
{
    const auto idToInfo = fromCameras(getCameras(cameraIds));
    const auto pathStoragePairs = genPaths(storages);
    const auto uniquePaths = toUniquePaths(pathStoragePairs);
    const bool storagePathsChanged = uniquePaths != m_paths;

    for (const auto& p: idToInfo)
    {
        const auto existingIt = m_idToInfo.find(p.first);
        if (existingIt == m_idToInfo.cend() || existingIt->second != p.second || storagePathsChanged)
            writeFile(p, pathStoragePairs);

        if (m_serverModule->commonModule()->isNeedToStop())
            break;
    }

    m_idToInfo = idToInfo;
    if (storagePathsChanged)
        m_paths = uniquePaths;
}

void Writer::write(
    const nx::vms::server::StorageResourceList& storages,
    const QString& cameraId,
    QnServer::ChunksCatalog quality)
{
    const auto camera =
        m_serverModule->resourcePool()->getResourceByUniqueId(cameraId).dynamicCast<QnVirtualCameraResource>();
    if (!camera)
    {
        NX_DEBUG(this, "Camera with unique id '%1' not found", cameraId);
        return;
    }

    writeFile({cameraId, infoFrom(camera)}, genPathsForQuality(storages, quality));
}

Reader::Reader(
    ReaderHandler* readerHandler,
    const QnAbstractStorageResource::FileInfo& fileInfo,
    std::function<QByteArray(const QString&)> getFileDataFunc)
    :
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

    NX_DEBUG(this, "Successfully read CamInfo data from %1", m_fileInfo->absoluteFilePath());
    outArchiveCameraList->push_back(m_archiveCamData);
}

bool Reader::initArchiveCamData()
{
    nx::vms::api::CameraData& coreData = m_archiveCamData.coreData;

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
        NX_DEBUG(
            this, "File data is NULL for archive camera %1", m_archiveCamData.coreData.physicalId);
    }

    return true;
}

static QString getVersion(QTextStream& stream)
{
    QString line;
    if (!stream.readLineInto(&line))
        return QString();

    const auto splits = line.split(" ");
    if (splits.size() != 2 || splits[0] != "VERSION")
        return QString();

    return splits[1];
}

class ParseResult
{
public:
    ParseResult(const QString& key, const QString& value):
        m_key(key),
        m_value(value)
    {}

    QString key() const { return m_key; }
    QString value() const { return m_value; }

private:
    QString m_key;
    QString m_value;
};

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

static bool readInnerData(
    const QString& src, char startBrace, char endBrace, QString* dst, bool include)
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

boost::optional<ParseResult> parseUnEncrypted(const QString& line)
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

    using ParseResult = ParseResult;
    return state == done ? boost::optional<ParseResult>(ParseResult(key, value)) : boost::none;
}

static boost::optional<ParseResult> parseEncrypted(const QString& line)
{
    const auto splits = line.split("=");
    if (splits.size() != 2)
        return boost::none;

    return ParseResult(
        nx::utils::decodeStringFromHexStringAES128CBC(splits[0]),
        nx::utils::decodeStringFromHexStringAES128CBC(splits[1]));
}

static boost::optional<ParseResult> parseLine(const QString& line, bool decrypt)
{
    if (decrypt)
        return parseEncrypted(line);

    return parseUnEncrypted(line);
}

bool Reader::parseData()
{
    QTextStream fileDataStream(&m_fileData);
    const auto version = getVersion(fileDataStream);
    if (version.isNull())
        fileDataStream.seek(0);

    QString line;
    while (fileDataStream.readLineInto(&line))
    {
        const auto result = parseLine(line, version >= "1.0");
        if (result)
            addProperty(*result);
    }

    return true;
}

void Reader::addProperty(const ParseResult& result)
{
    nx::vms::api::CameraData& coreData = m_archiveCamData.coreData;

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
            coreData.typeId = nx::vms::api::CameraData::kWearableCameraTypeId;
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
