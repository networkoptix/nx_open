#include <QtCore/QDir>
#include <QtCore/QCoreApplication>

#include <nx/kit/output_redirector.h>

#include <nx/kit/debug.h>

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

#include <nx/utils/app_info.h>

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
    const QString baseExeName = QFileInfo(exeName).baseName();
    qDebug() << "Usage:";
    qDebug().noquote() << " " << baseExeName << "[options] <cameraSet1> <cameraSet2> ... <cameraSetN>";
    qDebug() << "";
    qDebug() << "Here <cameraSet#> is a text without spaces containing semicolon-separated params:";
    qDebug() << " count=N";
    qDebug() << " files=\"<fileName>[,<fileName>...]\" - files for the primary stream; note the quotes";
    qDebug() << " secondary-files=\"<fileName>[,<fileName>...]\" - optional; files for the secondary stream; note the quotes";
    qDebug() << " offline=0..100 - optional, default is 0 - not offline";
    qDebug() << "";
    qDebug() << "[options]:";
    qDebug() << " -I, --local-interface=  Local interface to listen. By default, all interfaces are listened";
    qDebug() << " -S, --camera-for-file   Run separate camera for each primary file, count parameter must be empty or 0";
    qDebug() << " --pts                   Include original PTS into the stream";
    qDebug() << " --no-secondary          Do not stream the secondary stream";
    qDebug() << " --fps value             Force FPS for both primary and secondary streams to the given positive integer value";
    qDebug() << " --fps-primary value     Force FPS for the primary stream to the given positive integer value";
    qDebug() << " --fps-secondary value   Force FPS for the secondary stream to the given positive integer value";
    qDebug() << "";
    qDebug() << "Example:";
    qDebug().noquote() << " " << baseExeName << "files=\"c:/test.264\";count=20";
}

/**
 * @param argValue Null if the value arg is missing. Note: The last item in argv[] is always null.
 * @return -1 on invalid or missing value.
 */
int parsePositiveIntArg(const QString& argNameForErrorMessage, const char* argValue)
{
    if (argValue == nullptr)
    {
        qWarning() << "ERROR: Missing value after" << argNameForErrorMessage << "arg.";
        return -1;
    }

    const int value = QString(argValue).toInt();
    if (value <= 0)
    {
        qWarning() << "ERROR: Invalid value for" << argNameForErrorMessage << "arg (assuming -1):"
            << QString::fromStdString(nx::kit::utils::toString(argValue)); //< Enquote and escape.
        return -1;
    }

    return value;
}

int main(int argc, char *argv[])
{
    nx::kit::OutputRedirector::ensureOutputRedirection();

    QCoreApplication::setOrganizationName(nx::utils::AppInfo::organizationName());
    QCoreApplication::setApplicationName(nx::utils::AppInfo::vmsName() + " Test Camera");
    QCoreApplication::setApplicationVersion(nx::utils::AppInfo::applicationVersion());

    // Each user may have his/her own traytool running.
    QCoreApplication app(argc, argv);

    qDebug().noquote().nospace() << "\n"
        << qApp->applicationName() << " version " << qApp->applicationVersion()
        << "\n";

    if (argc == 1)
    {
        showUsage(argv[0]);
        return 1;
    }

    QnStaticCommonModule staticCommon(nx::vms::api::PeerType::notDefined, QString(), QString());
    //QnCommonModule common(false, nx::core::access::Mode::direct);

    QnFfmpegInitializer ffmpeg;

    bool cameraForEachFile = false;
    bool includePts = false;
    bool noSecondaryStream = false;
    int fpsPrimary = -1;
    int fpsSecondary = -1;
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
        else if( param == "--fps" )
        {
            ++i; //< The value is in the next arg.
            fpsPrimary = parsePositiveIntArg("--fps", argv[i]);
            fpsSecondary = fpsPrimary;
        }
        else if( param == "--fps-primary" )
        {
            ++i; //< The value is in the next arg.
            fpsPrimary = parsePositiveIntArg("--fps-primary", argv[i]);
        }
        else if( param == "--fps-secondary" )
        {
            ++i; //< The value is in the next arg.
            fpsSecondary = parsePositiveIntArg("--fps-secondary", argv[i]);
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

    auto storagePlugins = commonModule->storagePluginFactory();
    storagePlugins->registerStoragePlugin("file", QnQtFileStorageResource::instance, true);

    QnCameraPool::initGlobalInstance(new QnCameraPool(
        localInterfacesToListen, commonModule.get(), noSecondaryStream, fpsPrimary, fpsSecondary));
    QnCameraPool::instance()->start();
    for (int i = 1; i < argc; ++i)
    {
        QString param = argv[i];
        QStringList params = param.split(';');

        if( param.startsWith("--fps") )
        {
            ++i; //< Skipping the next arg.
            continue;
        }
        if( param.startsWith("--") )
        {
            continue;
        }
        if( param.startsWith("-") )
        {
            ++i; //< Skipping the next arg.
            continue;
        }

        int offlineFreq = 0;
        int count = 0;
        QString primaryFileNames;
        QString secondaryFileNames;

        for (int j = 0; j < params.size(); ++j)
        {
            QStringList data = params[j].split('=');
            if (data.size() < 2)
            {
                qWarning() << "Invalid parameter"
                    // Enquote and escape.
                    << QString::fromStdString(nx::kit::utils::toString(params[j].toStdString()))
                    << "; skipping.";
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
            qWarning() << "Parameter 'count' must be specified.";
            continue;
        }
        if (cameraForEachFile && count != 0) {
            qWarning() << "Parameter 'count' must not be specified with '--camera-for-file'.";
            continue;
        }
        if (primaryFileNames.isEmpty()) {
            qWarning() << "Parameter 'files=' must be specified.";
            continue;
        }

        const QStringList primaryFiles = checkFileNames(primaryFileNames);
        if (primaryFiles.isEmpty()) {
            qWarning() << "None of the files specified in 'files=' exist.";
            continue;
        }

        QStringList secondaryFiles;
        if (!secondaryFileNames.isEmpty())
            secondaryFiles = checkFileNames(secondaryFileNames);
        if (secondaryFiles.isEmpty())
            secondaryFiles = primaryFiles;

        QnCameraPool::instance()->addCameras(
            cameraForEachFile, includePts, count, primaryFiles, secondaryFiles, offlineFreq);
    }

    const int appResult = app.exec();

    delete QnCameraPool::instance();
    QnCameraPool::initGlobalInstance( NULL );

    qDebug() << "Exiting";
    return appResult;
}
