#include "applauncher_utils.h"

#include <QtNetwork/QLocalSocket>
#include <QtWidgets/QApplication>

#include "version.h"

#include <api/applauncher_api.h>
#include <api/ipc_pipe_names.h>


namespace applauncher
{
    static api::ResultType::Value sendCommandToLauncher(
        const applauncher::api::BaseTask& commandToSend,
        applauncher::api::Response* const response )
    {
        QLocalSocket sock;
        sock.connectToServer(launcherPipeName);
        if(!sock.waitForConnected(-1)) {
            qDebug() << "Failed to connect to local server" << launcherPipeName << sock.errorString();
            return api::ResultType::ioError;
        }

        //const QByteArray &serializedTask = applauncher::api::StartApplicationTask(version.toString(QnSoftwareVersion::MinorFormat), arguments).serialize();
        const QByteArray &serializedTask = commandToSend.serialize();
        if(sock.write(serializedTask.data(), serializedTask.size()) != serializedTask.size()) {
            qDebug() << "Failed to send launch task to local server" << launcherPipeName << sock.errorString();
            return api::ResultType::ioError;
        }

        sock.waitForReadyRead(-1);
        const QByteArray& responseData = sock.readAll();
        if( response )
            if( !response->deserialize( responseData ) )
                return api::ResultType::badResponse;
        return api::ResultType::ok;
    }

    api::ResultType::Value isVersionInstalled( const QnSoftwareVersion& version, bool* const installed )
    {
        api::IsVersionInstalledRequest request;
        request.version = version.toString(QnSoftwareVersion::MinorFormat);
        api::IsVersionInstalledResponse response;
        api::ResultType::Value result = sendCommandToLauncher( request, &response );
        if( result != api::ResultType::ok )
            return result;
        if( response.result != api::ResultType::ok )
            return response.result;
        *installed = response.installed;
        return api::ResultType::ok;
    }

    //bool canRestart(QnSoftwareVersion version) {
    //    if (version.isNull())
    //        version = QnSoftwareVersion(QN_ENGINE_VERSION);
    //    return QFile::exists(qApp->applicationDirPath() + QLatin1String("/../") + version.toString(QnSoftwareVersion::MinorFormat));
    //}

    api::ResultType::Value restartClient(QnSoftwareVersion version, const QByteArray &auth) {
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

        //return sendCommandToLauncher(version, arguments);
        return sendCommandToLauncher(
            applauncher::api::StartApplicationTask(
                version.toString(QnSoftwareVersion::MinorFormat),
                arguments.join(QLatin1String("")) ),
            nullptr );
    }

    api::ResultType::Value startInstallation(
        const QnSoftwareVersion& version,
        unsigned int* installationID )
    {
        api::StartInstallationTask request;
        request.version = version.toString(QnSoftwareVersion::MinorFormat);
        api::StartInstallationResponse response;
        api::ResultType::Value result = sendCommandToLauncher( request, &response );
        if( result != api::ResultType::ok )
            return result;
        if( response.result != api::ResultType::ok )
            return response.result;
        *installationID = response.installationID;
        return response.result;
    }

    api::ResultType::Value getInstallationStatus(
        unsigned int installationID,
        api::InstallationStatus::Value* const status,
        float* progress )
    {
        api::GetInstallationStatusRequest request;
        request.installationID = installationID;
        api::InstallationStatusResponse response;
        api::ResultType::Value result = sendCommandToLauncher( request, &response );
        if( result != api::ResultType::ok )
            return result;
        if( response.result != api::ResultType::ok )
            return response.result;
        *status = response.status;
        *progress = response.progress;
        return response.result;
    }

    api::ResultType::Value cancelInstallation( unsigned int installationID )
    {
        //TODO/IMPL
        return api::ResultType::otherError;
    }
}
