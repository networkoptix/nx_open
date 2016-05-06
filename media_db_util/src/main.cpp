#include <memory>
#include <QtCore>
#include "recorder/storage_db.h"
#include "recorder/storage_db_pool.h"
#include "plugins/storage/file_storage/file_storage_resource.h"
#include <core/resource_management/status_dictionary.h>
#include <common/common_module.h>
#include <core/resource_management/resource_properties.h>
#include <utils/common/util.h>
#include <utils/db/db_helper.h>

namespace aux
{
QTextStream outStream(stdout);

struct Config
{
    bool printCameraNames;
    bool printAllData;
    QStringList cameraNames;
    QString fileName;

    Config() : printCameraNames(false), printAllData(false) {}
};

void parseArgs(QCoreApplication &app, Config &cfg)
{
    QCommandLineParser parser;
    parser.addOptions({{{"f", "file-name"}, "Input DB file name. Required.", "file name"},
                       {{"p", "print-camera-names"}, "Print all cameras names and exit."},
                       {{"c", "camera-list"}, "Print data only for specific cameras.", "list"}});
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(app);

    if (parser.isSet("file-name"))
        cfg.fileName = parser.value("file-name");
    else
    {
        qCritical() << "DB file name is a required option";
        parser.showHelp();
        return;
    }

    cfg.printAllData = true;
    if (parser.isSet("print-camera-names"))
    {
        cfg.printCameraNames = true;
        cfg.printAllData = false;
    }
    else if (parser.isSet("camera-list"))
    {
        cfg.printAllData = false;
        cfg.cameraNames = parser.value("camera-list").split(" ");
        if (cfg.cameraNames.isEmpty())
        {
            qCritical() << "Cameras list is empty";
            QCoreApplication::exit(-1);
        }
    }
}

namespace detail
{
void printCameraName(const DeviceFileCatalogPtr &fileCatalog)
{
    outStream << fileCatalog->cameraUniqueId()
              << " (" << (fileCatalog->getCatalog() == QnServer::HiQualityCatalog ? "High quality" : "Low quality")
              << ")" << endl;
}

void printCameraData(const DeviceFileCatalogPtr &fileCatalog)
{
    printCameraName(fileCatalog);
    for (const auto &chunk : fileCatalog->getChunks())
        outStream << "\t{ startTime: " << chunk.startTimeMs << ", duration: " << chunk.durationMs
                  << ", storageIndex: " << chunk.storageIndex << ", timezone: " << chunk.timeZone
                  << ", fileSize: " << chunk.getFileSize() << " }" << endl;
    outStream << "\tTotal chunks: " << fileCatalog->getChunks().size() << endl << endl;
}

} // namespace detail

void printCameraNames(const QVector<DeviceFileCatalogPtr> &fileCatalogs)
{
    for (const auto &fileCatalog : fileCatalogs)
        detail::printCameraName(fileCatalog);
}

void printAllData(const QVector<DeviceFileCatalogPtr> &fileCatalogs)
{
    for (const auto &fileCatalog : fileCatalogs)
        detail::printCameraData(fileCatalog);
}


void printCamerasData(const QVector<DeviceFileCatalogPtr> &fileCatalogs, const QStringList &cameraNames)
{
    for (const QString& cameraName: cameraNames)
    {
        for (const auto &catalog: fileCatalogs)
            if (catalog->cameraUniqueId() == cameraName)
                detail::printCameraData(catalog);
    }
}

} // namespace aux

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("Media db util");
    QCoreApplication::setApplicationVersion("1.0");

    std::unique_ptr<QnCommonModule> commonModule = std::unique_ptr<QnCommonModule>(new QnCommonModule);
    commonModule->setModuleGUID(QnUuid("{A680980C-70D1-4545-A5E5-72D89E33648B}"));

    std::unique_ptr<QnResourceStatusDictionary> statusDictionary = std::unique_ptr<QnResourceStatusDictionary>(new QnResourceStatusDictionary);
    std::unique_ptr<QnResourcePropertyDictionary> propDictionary = std::unique_ptr<QnResourcePropertyDictionary>(new QnResourcePropertyDictionary);
    std::unique_ptr<QnStorageDbPool> dbPool = std::unique_ptr<QnStorageDbPool>(new QnStorageDbPool);


    aux::Config cfg;
    aux::parseArgs(app, cfg);

    QnFileStorageResourcePtr fileStorage(new QnFileStorageResource);
    fileStorage->setUrl(QFileInfo(cfg.fileName).absolutePath());
    if (!fileStorage->initOrUpdate())
    {
        qCritical() << "Failed to initialize file storage";
        return -1;
    }

    if (!fileStorage->isFileExists(cfg.fileName))
    {
        qCritical() << "DB file doesn't exist";
        return -1;
    }

    QnStorageDb db(fileStorage, 0);

    if (!db.open(cfg.fileName))
    {
        qCritical() << "Couldn't open database file.";
        QCoreApplication::exit(-1);
    }

    auto fileCatalogs = db.loadFullFileCatalog();

    if (cfg.printCameraNames)
        aux::printCameraNames(fileCatalogs);
    else if (cfg.printAllData)
        aux::printAllData(fileCatalogs);
    else if (!cfg.cameraNames.isEmpty())
        aux::printCamerasData(fileCatalogs, cfg.cameraNames);
}
