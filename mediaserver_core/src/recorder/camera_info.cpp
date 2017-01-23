#include <utils/common/util.h>
#include <nx/utils/log/log.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <recorder/device_file_catalog.h>
#include <recorder/storage_manager.h>
#include "camera_info.h"

namespace nx {
namespace caminfo {

QByteArray Composer::make(ComposerHandler* composerHandler)
{
    QByteArray result;
    if (!composerHandler->hasCamera())
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


Writer::Writer(WriterHandler* writeHandler, std::chrono::milliseconds writeInterval):
    m_handler(writeHandler),
    m_writeInterval(writeInterval),
    m_lastWriteTime(std::chrono::time_point<std::chrono::steady_clock>::min())
{}

void Writer::write()
{
    auto now = std::chrono::steady_clock::now();
    if (now - m_writeInterval < m_lastWriteTime)
        return;

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

    m_lastWriteTime = std::chrono::steady_clock::now();
}

void Writer::writeInfoIfNeeded(const QString& infoFilePath, const QByteArray& infoFileData)
{
    if (infoFilePath.isEmpty() || infoFileData.isEmpty())
        return;

    if (!m_infoPathToCameraInfo.contains(infoFilePath) ||
        m_infoPathToCameraInfo[infoFilePath] != infoFileData)
    {
        if (m_handler->replaceFile(infoFilePath, infoFileData))
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

void Writer::setWriteInterval(std::chrono::milliseconds interval)
{
    m_writeInterval = interval;
}


ServerHandler::ServerHandler(QnStorageManager* storageManager):
    m_storageManager(storageManager)
{}

QStringList ServerHandler::storagesUrls() const
{
    QStringList result;
    for (const auto& storage: m_storageManager->getUsedWritableStorages())
        result.append(storage->getUrl());
    return result;
}

QStringList ServerHandler::camerasIds(QnServer::ChunksCatalog catalog) const
{
    return m_storageManager->getAllCameraIdsUnderLock(catalog);
}

bool ServerHandler::needStop() const
{
    return QnResource::isStopping();
}

bool ServerHandler::replaceFile(const QString& path, const QByteArray& data)
{
    auto storage = m_storageManager->getStorageByUrlInternal(path);
    NX_ASSERT(storage);
    if (!storage)
        return false;

    if (!storage->removeFile(path))
    {
        NX_LOG(lit("%1. Remove camera info file failed for this path: %2")
                .arg(Q_FUNC_INFO)
                .arg(path), cl_logDEBUG1);
        return false;
    }
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

QString ServerHandler::name() const
{
    if (!hasCamera())
        return QString();
    return m_camera->getUserDefinedName();
}

QString ServerHandler::model() const
{
    if (!hasCamera())
        return QString();
    return m_camera->getModel();
}

QString ServerHandler::groupId() const
{
    if (!hasCamera())
        return QString();
    return m_camera->getGroupId();
}

QString ServerHandler::groupName() const
{
    if (!hasCamera())
        return QString();
    return m_camera->getGroupName();
}

QString ServerHandler::url() const
{
    if (!hasCamera())
        return QString();
    return m_camera->getUrl();
}

QList<QPair<QString, QString>> ServerHandler::properties() const
{
    QList<QPair<QString, QString>> result;
    if (!hasCamera())
        return result;

    for (const auto& prop: m_camera->getAllProperties())
        result.append(QPair<QString, QString>(prop.name, prop.value));

    return result;
}

ComposerHandler* ServerHandler::composerHandler(const QString& cameraId)
{
    m_camera = qnResPool->getResourceByUniqueId<QnSecurityCamResource>(cameraId);
    return this;
}
bool ServerHandler::hasCamera() const
{
    return static_cast<bool>(m_camera);
}

} //namespace caminfo
} //namespace nx
