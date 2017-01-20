#include <utils/common/util.h>
#include <recorder/device_file_catalog.h>
#include "camera_info.h"

namespace nx {
namespace caminfo {

QByteArray Composer::make(ComposerHandler* composerHandler)
{
    QByteArray result;
    m_stream.reset(new QTextStream(&result));
    m_handler = composerHandler;

    printKeyValue(kArchiveCameraNameKey, m_handler->name());
    printKeyValue(kArchiveCameraModelKey, m_handler->model());
    printKeyValue(kArchiveCameraGroupIdKey, m_handler->groupId());
    printKeyValue(kArchiveCameraGroupNameKey, m_handler->groupName());
    printKeyValue(kArchiveCameraUrlKey, m_handler->name());

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
    m_handler(writeHandler),
    m_lastWriteTime(std::chrono::time_point<std::chrono::steady_clock>::min())
{}

void Writer::write()
{
    auto now = std::chrono::steady_clock::now();
    if (now - kWriteInfoFilesInterval < m_lastWriteTime)
        return;

    for (auto& storageUrl: m_handler->storagesUrls())
        for (int i = 0; i < static_cast<int>(QnServer::ChunksCatalogCount); ++i)
            for (auto& cameraId: m_handler->camerasIds())
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
    auto basePath = closeDirPath(storageUrl + DeviceFileCatalog::prefixByCatalog(catalog) + separator);
    return basePath + cameraId + separator + lit("info.txt");
}

}
}
