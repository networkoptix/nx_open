#include "mainwindow.h"

#include <QtGui/QApplication>
#include <QFile>
#include <QDir>
#include <QDebug>
#include "common/log.h"
#include "common/util.h"

const char* const ORGANIZATION_NAME="Network Optix";
const char* const APPLICATION_NAME="VMS client";
const char* const APPLICATION_VERSION="0.5.0";

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(images);

    QApplication::setOrganizationName(QLatin1String(ORGANIZATION_NAME));
    QApplication::setApplicationName(QLatin1String(APPLICATION_NAME));
    QApplication::setApplicationVersion(QLatin1String(APPLICATION_VERSION));

    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    QString dataLocation = getDataDirectory();
    QDir::setCurrent(QFileInfo(QFile::decodeName(argv[0])).absolutePath());
    QDir dataDirectory;
    dataDirectory.mkpath(dataLocation + QLatin1String("/log"));
    if (!cl_log.create(dataLocation + QLatin1String("/log/log_file"), 1024*1024*10, 5, cl_logDEBUG1))
    {
        app.quit();
        return 0;
    }

    MainWindow window;
    window.show();

    return app.exec();
}
