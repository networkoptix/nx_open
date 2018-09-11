#include "applauncher_utils.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client/client_startup_parameters.h>
#include <client/self_updater.h>
#include <api/applauncher_api.h>
#include <api/ipc_pipe_names.h>
#include <utils/ipc/named_pipe_socket.h>
#include <nx/utils/log/log.h>

namespace applauncher {

namespace {

constexpr int MAX_MSG_LEN = 1024 * 64; //64K ought to be enough for anybody
constexpr int kDefaultTimeoutMs = 3000;
constexpr int kZipInstallationTimeoutMs = 30000;

struct FunctionsTag{};

} // namespace

static api::ResultType::Value sendCommandToLauncher(
    const applauncher::api::BaseTask& commandToSend,
    applauncher::api::Response* const response,
    int timeoutMs = kDefaultTimeoutMs)
{
    NamedPipeSocket sock;
    SystemError::ErrorCode resultCode = sock.connectToServerSync(launcherPipeName());
    if( resultCode != SystemError::noError )
    {        NX_WARNING (typeid(FunctionsTag), lit("Failed to connect to local server %1. %2").arg(launcherPipeName()).arg(SystemError::toString(resultCode)));
        return api::ResultType::connectError;
    }

    //const QByteArray &serializedTask = applauncher::api::StartApplicationTask(
    //    version.toString(nx::utils::SoftwareVersion::MinorFormat), arguments).serialize();
    const QByteArray& serializedTask = commandToSend.serialize();
    unsigned int bytesWritten = 0;
    resultCode = sock.write(serializedTask.data(), serializedTask.size(), &bytesWritten);
    if( resultCode != SystemError::noError )
    {
        NX_WARNING (typeid(FunctionsTag), lit("Failed to send launch task to local server %1. %2").arg(launcherPipeName()).arg(SystemError::toString(resultCode)));
        return api::ResultType::connectError;
    }

    char buf[MAX_MSG_LEN];
    unsigned int bytesRead = 0;
    resultCode = sock.read(buf, sizeof(buf), &bytesRead, timeoutMs);  //ignoring return code
    if( resultCode != SystemError::noError )
    {
        NX_WARNING (typeid(FunctionsTag), lit("Failed to read response from local server %1. %2").arg(launcherPipeName()).arg(SystemError::toString(resultCode)));
        return api::ResultType::connectError;
    }
    if( response )
        if( !response->deserialize( QByteArray::fromRawData(buf, bytesRead) ) )
            return api::ResultType::badResponse;
    return api::ResultType::ok;
}

api::ResultType::Value isVersionInstalled(
    nx::utils::SoftwareVersion version, bool* const installed)
{
    if (version.isNull())
        version = qnStaticCommon->engineVersion();

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

//bool canRestart(nx::utils::SoftwareVersion version) {
//    if (version.isNull())
//        version = nx::utils::SoftwareVersion(qnStaticCommon->engineVersion());
//    return QFile::exists(qApp->applicationDirPath() + QLatin1String("/../")
//      + version.toString(nx::utils::SoftwareVersion::MinorFormat));
//}

api::ResultType::Value restartClient(nx::utils::SoftwareVersion version, const QString& auth)
{
    if (version.isNull())
        version = nx::utils::SoftwareVersion(qnStaticCommon->engineVersion());

    QStringList arguments;
    arguments << QLatin1String("--no-single-application");
    if (!auth.isEmpty()) {
        arguments << QLatin1String("--auth");
        arguments << auth;
    }
    arguments << QnStartupParameters::kScreenKey;
    arguments << QString::number(qApp->desktop()->screenNumber(qApp->activeWindow()));

    api::Response response;

    //return sendCommandToLauncher(version, arguments);
    const api::ResultType::Value result = sendCommandToLauncher(
        applauncher::api::StartApplicationTask(version, arguments),
        &response);
    return result != api::ResultType::ok ? result : response.result;
}

api::ResultType::Value startInstallation(
    const nx::utils::SoftwareVersion& version,
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
    const nx::utils::SoftwareVersion& version,
    const QString &zipFileName )
{
    api::InstallZipTask request;
    request.version = version;
    request.zipFileName = zipFileName;
    api::Response response;
    const auto result = sendCommandToLauncher(request, &response, kZipInstallationTimeoutMs);
    if (result != api::ResultType::ok)
        return result;
    if (response.result != api::ResultType::ok)
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

applauncher::api::ResultType::Value quitApplauncher()
{
    api::QuitTask task;
    api::Response response;
    api::ResultType::Value result = sendCommandToLauncher(task, &response);
    if (result != api::ResultType::ok)
        return result;
    if (response.result != api::ResultType::ok)
        return response.result;
    return response.result;
}

api::ResultType::Value getInstalledVersions(QList<nx::utils::SoftwareVersion>* versions)
{
    api::GetInstalledVersionsRequest request;
    api::GetInstalledVersionsResponse response;
    api::ResultType::Value result = sendCommandToLauncher(request, &response);
    if( result != api::ResultType::ok )
        return result;
    *versions = response.versions;
    return response.result;
}

bool checkOnline(bool runWhenOffline)
{
    /* Try to run applauncher if it is not running. */
    static const nx::utils::SoftwareVersion anyVersion(3, 0);
    bool notUsed = false;
    const auto result = isVersionInstalled(anyVersion, &notUsed);

    if (result == api::ResultType::ok)
        return true;

    return ((result == api::ResultType::connectError) && runWhenOffline
        && nx::vms::client::SelfUpdater::runMinilaucher());
}

} // namespace applauncher
