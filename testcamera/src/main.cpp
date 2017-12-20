#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QCoreApplication>

#include "core/storage/file_storage/qtfile_storage_resource.h"

#include "common/common_module.h"
#include <common/static_common_module.h>

#include "utils/common/synctime.h"
#include "core/resource_management/status_dictionary.h"
#include "core/resource/resource_fwd.h"
#include <nx/core/access/access_types.h>
#include "core/resource/camera_user_attribute_pool.h"
#include "api/global_settings.h"
#include "core/resource_management/resource_properties.h"
#include "core/resource/storage_plugin_factory.h"

#include "camera_pool.h"

#include <utils/media/ffmpeg_initializer.h>
#include <nx/core/access/access_types.h>

extern "C"
{
#include <libavformat/avformat.h>
}

#include <utils/common/app_info.h>

QString doUnquote(const QString& fileName)
{
    QString rez = fileName;
    if (rez.startsWith("\"") || rez.startsWith("'"))
        rez = rez.mid(1);
    if (rez.endsWith("\"") || rez.endsWith("'"))
        rez = rez.left(rez.length()-1);
    return rez;
}

QStringList checkFileNames(const QString& fileNames)
{
    QStringList files;
    for (auto& file: fileNames.split(',')) {
        if (!QFile::exists(file))
            qWarning() << "File" << file << "not found";
        else
            files.append(file);
    }
    return files;
}

void showUsage(char* exeName)
{
    qDebug() << "usage:";
    qDebug() << "testCamera [options] <cameraSet1> <cameraSet2> ... <cameraSetN>";
    qDebug() << "where <cameraSetN> is semicolon-separated camera param(s):";
    qDebug() << "count=N";
    qDebug() << "files=\"<fileName>[,<fileName>...]\" - for primary stream";
    qDebug() << "secondary-files=\"<fileName>[,<fileName>...]\" - for low quality stream";
    qDebug() << "[offline=0..100] (optional, default is 0 - no offline)";
    qDebug() << "[fps=<value>] (optional, default is 10; ATTENTION: Video file FPS is ignored)";
    qDebug() << "[primary=0|1] (optional, default is 0)";
    qDebug() << "";
    qDebug() << "example:";
    QString str = QFileInfo(exeName).baseName() + QString(" files=\"c:/test.264\";count=20");
    qDebug().noquote() << str;
    qDebug() << "\n[options]: ";
    qDebug() << "-I, --local-interface=     Local interface to listen. By default, all interfaces are listened";
    qDebug() << "-S, --camera-for-file      Run separate camera for each primary file, count parameter must be empty or 0";
    qDebug() << "--pts                      Include original PTS into the stream";
    qDebug() << "--no-secondary             Do not stream the secondary stream";
}


int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QnAppInfo::organizationName());
    QCoreApplication::setApplicationName("Nx Witness Test Camera");
    QCoreApplication::setApplicationVersion(QnAppInfo::applicationVersion());



    // Each user may have it's own traytool running.
    QCoreApplication app(argc, argv);

    qDebug() << qApp->applicationName() << "version" << qApp->applicationVersion();

    if (argc == 1)
    {
        showUsage(argv[0]);
        return 1;
    }

    QnStaticCommonModule staticCommon(Qn::PT_NotDefined, QString(), QString());
    //QnCommonModule common(false, nx::core::access::Mode::direct);

    QnFfmpegInitializer ffmpeg;
    QnLongRunnablePool logRunnablePool;
    //common.instance<QnFfmpegInitializer>();
    //common.instance<QnLongRunnablePool>();
    QnStoragePluginFactory::instance()->registerStoragePlugin("file",
        QnQtFileStorageResource::instance, true);


    bool cameraForEachFile = false;
    bool includePts = false;
    bool noSecondaryStream = false;
    QStringList localInterfacesToListen;
    for( int i = 1; i < argc; ++i )
    {
        QString param = argv[i];
        if( param.startsWith("--local-interface=") )
        {
            localInterfacesToListen.push_back( param.mid(sizeof("--local-interface=")-1) );
        }
        else if( param == "-I" )
        {
            //value in the next arg
            ++i;
            if( i >= argc )
                continue;
            localInterfacesToListen.push_back( QString(argv[i]) );
        }
        else if( param == "--camera-for-file" || param == "-S" )
        {
            cameraForEachFile = true;
        }
        else if( param == "--no-secondary" )
        {
            noSecondaryStream = true;
        }
        else if( param == "--pts" )
        {
            includePts = true;
        }
    }

    std::unique_ptr<QnCommonModule> commonModule(
        new QnCommonModule(/*clientMode*/ false, nx::core::access::Mode::direct));
    QnCameraPool::initGlobalInstance(new QnCameraPool(
        localInterfacesToListen, commonModule.get(), noSecondaryStream));
    QnCameraPool::instance()->start();
    for (int i = 1; i < argc; ++i)
    {
        QString param = argv[i];
        QStringList params = param.split(';');

        if( param.startsWith("--") )
            continue;
        if( param.startsWith("-") )
        {
            ++i;    //skipping next argument
            continue;
        }

        int offlineFreq = 0;
        int count = 0;
        QString primaryFileNames;
        QString secondaryFileNames;
        QStringList primaryFiles;
        QStringList secondaryFiles;

        for (int j = 0; j < params.size(); ++j)
        {
            QStringList data = params[j].split('=');
            if (data.size() < 2)
            {
                qWarning() << "invalid param" << params[j] << "skip.";
                continue;
            }
            data[0] = data[0].toLower().trimmed();
            while (!data[0].isEmpty() && data[0] == "-")
                data[0] = data[0].mid(1);

            if (data[0] == "offline")
                offlineFreq = data[1].toInt();
            else if (data[0] == "count")
                count = data[1].toInt();
            else if (data[0] == "files")
                primaryFileNames = doUnquote(data[1]);
            else if (data[0] == "secondary-files")
                secondaryFileNames = doUnquote(data[1]);
        }
        if (!cameraForEachFile && count == 0) {
            qWarning() << "Parameter 'count' must be specified";
            continue;
        }
        if (cameraForEachFile && count != 0) {
            qWarning() << "Parameter 'count' must not be specified when creating separate camera for each file";
            continue;
        }
        if (primaryFileNames.isEmpty()) {
            qWarning() << "Parameter 'files' must be specified";
            continue;
        }

        primaryFiles = checkFileNames(primaryFileNames);
        if (primaryFiles.isEmpty()) {
            qWarning() << "No one of the specified files exists!";
            continue;
        }

        if (!secondaryFileNames.isEmpty())
            secondaryFiles = checkFileNames(secondaryFileNames);
        if (secondaryFiles.isEmpty())
            secondaryFiles = primaryFiles;

        QnCameraPool::instance()->addCameras(
            cameraForEachFile, includePts, count, primaryFiles, secondaryFiles, offlineFreq);
    }

    int appResult = app.exec();

    delete QnCameraPool::instance();
    QnCameraPool::initGlobalInstance( NULL );

    qDebug() << "Exiting!!!!!!!!!!!!!!!!!!!!!!!!!!";
    return appResult;
}
