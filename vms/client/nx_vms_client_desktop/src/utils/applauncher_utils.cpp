#include "applauncher_utils.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client/client_startup_parameters.h>
#include <client/self_updater.h>

#include <nx/vms/applauncher/api/commands.h>

#include <nx/utils/log/log.h>

namespace applauncher {
namespace api {

namespace {

} // namespace

static const int kZipInstallationTimeoutMs = 30000;

ResultType::Value isVersionInstalled(
    const nx::utils::SoftwareVersion& version,
    bool* const installed)
{
    IsVersionInstalledRequest request;
    request.version = version;
    IsVersionInstalledResponse response;
    ResultType::Value result = sendCommandToApplauncher(request, &response);
    if (result != ResultType::ok)
        return result;
    if (response.result != ResultType::ok)
        return response.result;
    *installed = response.installed;
    return ResultType::ok;
}

ResultType::Value restartClient(
    nx::utils::SoftwareVersion version,
    const QString& auth)
{
    QStringList arguments;
    arguments << QLatin1String("--no-single-application");
    if (!auth.isEmpty())
    {
        arguments << QLatin1String("--auth");
        arguments << auth;
    }
    arguments << QnStartupParameters::kScreenKey;
    arguments << QString::number(qApp->desktop()->screenNumber(qApp->activeWindow()));

    Response response;

    const ResultType::Value result = sendCommandToApplauncher(
        StartApplicationTask(version, arguments),
        &response);
    return result != ResultType::ok ? result : response.result;
}

ResultType::Value installZip(
    const nx::utils::SoftwareVersion& version,
    const QString& zipFileName)
{
    InstallZipTask request;
    request.version = version;
    request.zipFileName = zipFileName;
    Response response;
    const auto result = sendCommandToApplauncher(request, &response, kZipInstallationTimeoutMs);
    if (result != ResultType::ok)
        return result;
    if (response.result != ResultType::ok)
        return response.result;
    return response.result;
}

ResultType::Value scheduleProcessKill(qint64 processID, quint32 timeoutMillis)
{
    AddProcessKillTimerRequest request;
    request.processID = processID;
    request.timeoutMillis = timeoutMillis;
    AddProcessKillTimerResponse response;
    ResultType::Value result = sendCommandToApplauncher(request, &response);
    if (result != ResultType::ok)
        return result;
    return response.result;
}

ResultType::Value quitApplauncher()
{
    QuitTask task;
    Response response;
    ResultType::Value result = sendCommandToApplauncher(task, &response);
    if (result != ResultType::ok)
        return result;
    if (response.result != ResultType::ok)
        return response.result;
    return response.result;
}

ResultType::Value getInstalledVersions(QList<nx::utils::SoftwareVersion>* versions)
{
    GetInstalledVersionsRequest request;
    GetInstalledVersionsResponse response;
    ResultType::Value result = sendCommandToApplauncher(request, &response);
    if (result != ResultType::ok)
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

    if (result == ResultType::ok)
        return true;

    return ((result == ResultType::connectError) && runWhenOffline
        && nx::vms::client::SelfUpdater::runMinilaucher());
}

} // namespace api
} // namespace applauncher
