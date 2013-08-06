#include "applauncher_utils.h"

#include <QtGui/QApplication>

#include <QtNetwork/QLocalSocket>

#include "version.h"

#include <api/ipc_pipe_names.h>
#include <api/start_application_task.h>

bool sendCommandToLauncher(const QnSoftwareVersion &version, const QStringList &arguments) {
    QLocalSocket sock;
    sock.connectToServer(launcherPipeName);
    if(!sock.waitForConnected(-1)) {
        qDebug() << "Failed to connect to local server" << launcherPipeName << sock.errorString();
        return false;
    }

    const QByteArray &serializedTask = applauncher::api::StartApplicationTask(version.toString(QnSoftwareVersion::MinorFormat), arguments).serialize();
    if(sock.write(serializedTask.data(), serializedTask.size()) != serializedTask.size()) {
        qDebug() << "Failed to send launch task to local server" << launcherPipeName << sock.errorString();
        return false;
    }

    sock.waitForReadyRead(-1);
    sock.readAll();
    return true;
}

bool canRestart(QnSoftwareVersion version) {
    if (version.isNull())
        version = QnSoftwareVersion(QN_ENGINE_VERSION);
    return QFile::exists(qApp->applicationDirPath() + QLatin1String("/../") + version.toString(QnSoftwareVersion::MinorFormat));
}

bool restartClient(QnSoftwareVersion version, const QByteArray &auth) {
    if (version.isNull())
        version = QnSoftwareVersion(QN_ENGINE_VERSION);

    QStringList arguments;
    arguments << QLatin1String("--no-single-application");
    if (!auth.isEmpty()) {
        arguments << QLatin1String("--auth");
        arguments << QLatin1String(auth);
    }
    arguments << QLatin1String("--screen");
    arguments << QString::number(qApp->desktop()->screenNumber(qApp->activeWindow()));

    return sendCommandToLauncher(version, arguments);
}
