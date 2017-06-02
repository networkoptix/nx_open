#define _WINSOCKAPI_

#include <memory>
#include <QtCore>
#include "recorder/storage_db.h"
#include "recorder/storage_db_pool.h"
#include "plugins/storage/file_storage/file_storage_resource.h"
#include <core/resource_management/status_dictionary.h>

#include <common/static_common_module.h>
#include <common/common_module.h>

#include <core/resource_management/resource_properties.h>
#include <utils/common/util.h>
#include <utils/common/writer_pool.h>
#include <utils/db/db_helper.h>
#include <media_server/settings.h>

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
    for (const auto &chunk : fileCatalog->getChunksUnsafe())
        outStream << "\t{ startTime: " << chunk.startTimeMs << ", duration: " << chunk.durationMs
                  << ", storageIndex: " << chunk.storageIndex << ", timezone: " << chunk.timeZone
                  << ", fileSize: " << chunk.getFileSize() << " }" << endl;
    outStream << "\tTotal chunks: " << fileCatalog->getChunksUnsafe().size() << endl << endl;
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

void messageHandler(QtMsgType type, const QMessageLogContext &/*context*/, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s\n", localMsg.constData());
        break;
    case QtInfoMsg:
        fprintf(stderr, "Info: %s\n", localMsg.constData());
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s\n", localMsg.constData());
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s\n", localMsg.constData());
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s\n", localMsg.constData());
        exit(-1);
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("Media db util");
    QCoreApplication::setApplicationVersion("1.0");
    qInstallMessageHandler(&messageHandler);

    QnStaticCommonModule staticCommon;
    std::unique_ptr<QnCommonModule> commonModule = std::unique_ptr<QnCommonModule>(
        new QnCommonModule(true));
    commonModule->setModuleGUID(QnUuid("{A680980C-70D1-4545-A5E5-72D89E33648B}"));
//
//     std::unique_ptr<QnResourceStatusDictionary> statusDictionary = std::unique_ptr<QnResourceStatusDictionary>(new QnResourceStatusDictionary);
//     std::unique_ptr<QnResourcePropertyDictionary> propDictionary = std::unique_ptr<QnResourcePropertyDictionary>(new QnResourcePropertyDictionary);
    std::unique_ptr<QnStorageDbPool> dbPool = std::unique_ptr<QnStorageDbPool>(
        new QnStorageDbPool(commonModule.get()));
    //MSSettings::initializeROSettings();
    QnWriterPool writerPool;


    aux::Config cfg;
    aux::parseArgs(app, cfg);

    QnFileStorageResourcePtr fileStorage(new QnFileStorageResource(commonModule.get()));
    fileStorage->setUrl(QFileInfo(cfg.fileName).absolutePath());
    if (fileStorage->initOrUpdate() != Qn::StorageInit_Ok)
        qFatal("Failed to initialize file storage");

    if (!fileStorage->isFileExists(cfg.fileName))
        qFatal("DB file doesn't exist");

    QnStorageDb db(fileStorage, 0);

    if (!db.open(cfg.fileName))
        qFatal("Couldn't open database file.");

    auto fileCatalogs = db.loadFullFileCatalog();

    if (cfg.printCameraNames)
        aux::printCameraNames(fileCatalogs);
    else if (cfg.printAllData)
        aux::printAllData(fileCatalogs);
    else if (!cfg.cameraNames.isEmpty())
        aux::printCamerasData(fileCatalogs, cfg.cameraNames);
}
