#include "version.h"

#include <QApplication>
#include <QDir>
#include <QSettings>

#include "camera_pool.h"
#include "plugins/storage/file_storage/qtfile_storage_resource.h"
#include "utils/common/module_resources.h"


QSettings qSettings;	//TODO/FIXME remove this shit. Have to add to build common as shared object, since it requires extern qSettibns to be defined somewhere...

QString doUnquote(const QString& fileName)
{
    QString rez = fileName;
    if (rez.startsWith("\"") || rez.startsWith("'"))
        rez = rez.mid(1);
    if (rez.endsWith("\"") || rez.endsWith("'"))
        rez = rez.left(rez.length()-1);
    return rez;
}

void ffmpegInit()
{
    av_register_all();

    QnStoragePluginFactory::instance()->registerStoragePlugin("file", QnQtFileStorageResource::instance, true);
    QnStoragePluginFactory::instance()->registerStoragePlugin("qtfile", QnQtFileStorageResource::instance);
}

int main(int argc, char *argv[])
{
    QN_INIT_MODULE_RESOURCES(common);

    QApplication::setOrganizationName(QLatin1String(QN_ORGANIZATION_NAME));
    QApplication::setApplicationName(QLatin1String(QN_APPLICATION_NAME));
    QApplication::setApplicationVersion(QLatin1String(QN_APPLICATION_VERSION));

    ffmpegInit();  
    
    // Each user may have it's own traytool running.
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    QDir::setCurrent(QFileInfo(QFile::decodeName(argv[0])).absolutePath());

    qDebug() << QN_APPLICATION_NAME << "version" << QN_APPLICATION_VERSION;
    
    if (argc == 1)
    {
        qDebug() << "usage:";
        qDebug() << "testCamera <cameraSet1> <cameraSet2> ... <cameraSetN>";
        qDebug() << "where <cameraSetN> is camera(s) param with ';' delimiter";
        qDebug() << "count=N";
        qDebug() << "files=\"<fileName>[,<fileName>...]\" - for primary stream";
        qDebug() << "secondary-files=\"<fileName>[,<fileName>...]\" - for low quality stream";
        qDebug() << "[offline=0.100] (optional, default value 0 - no offline)";
        qDebug() << "";
        qDebug() << "example:";
        QString str = QFileInfo(argv[0]).baseName() + QString(" files=\"c:/test.264\";count=20");
        qDebug() << str;
        return 1;
    }

    QnCameraPool::instance()->start();

    for (int i = 1; i < argc; ++i)
    {
        QString param = argv[i];
        QStringList params = param.split(';');

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
        if (count == 0) {
            qWarning() << "Parameter 'count' must be specified";
            continue;
        }
        if (primaryFileNames.isEmpty()) {
            qWarning() << "Parameter 'files' must be specified";
            continue;
        }


        primaryFiles = primaryFileNames.split(',');
        for (int k = 0; k < primaryFiles.size(); ++k)
        if (!QFile::exists(primaryFiles[k])) {
            qWarning() << "File" << primaryFiles[k] << "not found";
            continue;
        }

        if (!secondaryFileNames.isEmpty())
            secondaryFiles = secondaryFileNames.split(',');
        for (int k = 0; k < secondaryFiles.size(); ++k)
            if (!QFile::exists(secondaryFiles[k])) {
                qWarning() << "File" << secondaryFiles[k] << "not found";
                continue;
            }

        if(secondaryFiles.isEmpty())
            secondaryFiles = primaryFiles;

        QnCameraPool::instance()->addCameras(count, primaryFiles, secondaryFiles, offlineFreq);
    }


    return app.exec();
}
