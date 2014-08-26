
#include "applauncher_utils.h"

#include <QtWidgets/QApplication>

#include "version.h"

#include <api/applauncher_api.h>
#include <api/ipc_pipe_names.h>
#include <utils/ipc/named_pipe_socket.h>
#include <utils/common/log.h>


namespace applauncher
{
    static const int MAX_MSG_LEN = 1024;

    static api::ResultType::Value sendCommandToLauncher(
        const applauncher::api::BaseTask& commandToSend,
        applauncher::api::Response* const response )
    {
        NamedPipeSocket sock;
        SystemError::ErrorCode resultCode = sock.connectToServerSync( launcherPipeName );
        if( resultCode != SystemError::noError )
        {
            NX_LOG( lit("Failed to connect to local server %1. %2").arg(launcherPipeName).arg(SystemError::toString(resultCode)), cl_logWARNING );
            return api::ResultType::connectError;
        }

        //const QByteArray &serializedTask = applauncher::api::StartApplicationTask(version.toString(QnSoftwareVersion::MinorFormat), arguments).serialize();
        const QByteArray& serializedTask = commandToSend.serialize();
        unsigned int bytesWritten = 0;
        resultCode = sock.write(serializedTask.data(), serializedTask.size(), &bytesWritten);
        if( resultCode != SystemError::noError )
        {
            NX_LOG( lit("Failed to send launch task to local server %1. %2").arg(launcherPipeName).arg(SystemError::toString(resultCode)), cl_logWARNING );
            return api::ResultType::connectError;
        }

        char buf[MAX_MSG_LEN];
        unsigned int bytesRead = 0;
        resultCode = sock.read( buf, sizeof(buf), &bytesRead );  //ignoring return code
        if( resultCode != SystemError::noError )
        {
            NX_LOG( lit("Failed to read response from local server %1. %2").arg(launcherPipeName).arg(SystemError::toString(resultCode)), cl_logWARNING );
            return api::ResultType::connectError;
        }
        if( response )
            if( !response->deserialize( QByteArray::fromRawData(buf, bytesRead) ) )
                return api::ResultType::badResponse;
        return api::ResultType::ok;
    }

    api::ResultType::Value isVersionInstalled( QnSoftwareVersion version, bool* const installed )
    {
        if (version.isNull())
            version = QnSoftwareVersion(QN_ENGINE_VERSION);

        api::IsVersionInstalledRequest request;
        request.version = version;
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

        api::Response response;

        //return sendCommandToLauncher(version, arguments);
        const api::ResultType::Value result = sendCommandToLauncher(
            applauncher::api::StartApplicationTask(version, arguments),
            &response);
        return result != api::ResultType::ok ? result : response.result;
    }

    api::ResultType::Value startInstallation(
        const QnSoftwareVersion& version,
        unsigned int* installationID )
    {
        api::StartInstallationTask request;
        request.version = version;
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

    api::ResultType::Value installZip(
        const QnSoftwareVersion &version,
        const QString &zipFileName )
    {
        api::InstallZipTask request;
        request.version = version;
        request.zipFileName = zipFileName;
        api::Response response;
        api::ResultType::Value result = sendCommandToLauncher( request, &response );
        if( result != api::ResultType::ok )
            return result;
        if( response.result != api::ResultType::ok )
            return response.result;
        return response.result;
    }

    api::ResultType::Value cancelInstallation( unsigned int installationID )
    {
        api::CancelInstallationRequest request;
        request.installationID = installationID;
        api::CancelInstallationResponse response;
        api::ResultType::Value result = sendCommandToLauncher( request, &response );
        if( result != api::ResultType::ok )
            return result;
        return response.result;
    }

    api::ResultType::Value scheduleProcessKill( qint64 processID, quint32 timeoutMillis )
    {
        api::AddProcessKillTimerRequest request;
        request.processID = processID;
        request.timeoutMillis = timeoutMillis;
        api::AddProcessKillTimerResponse response;
        api::ResultType::Value result = sendCommandToLauncher( request, &response );
        if( result != api::ResultType::ok )
            return result;
        return response.result;
    }

    api::ResultType::Value getInstalledVersions(QList<QnSoftwareVersion> *versions)
    {
        api::GetInstalledVersionsRequest request;
        api::GetInstalledVersionsResponse response;
        api::ResultType::Value result = sendCommandToLauncher( request, &response );
        if( result != api::ResultType::ok )
            return result;
        *versions = response.versions;
        return response.result;
    }

}
