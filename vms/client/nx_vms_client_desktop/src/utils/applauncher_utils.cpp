#include "applauncher_utils.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client/client_startup_parameters.h>
#include <client/self_updater.h>

#include <nx/vms/applauncher/api/commands.h>

#include <nx/utils/log/log.h>

namespace nx::applauncher::api {

namespace {

static const int kZipInstallationTimeoutMs = 30000;

} // namespace

ResultType isVersionInstalled(
    const nx::utils::SoftwareVersion& version,
    bool* const installed)
{
    IsVersionInstalledRequest request;
    request.version = version;
    IsVersionInstalledResponse response;
    ResultType result = sendCommandToApplauncher(request, &response);
    if (result != ResultType::ok)
        return result;
    if (response.result != ResultType::ok)
        return response.result;
    *installed = response.installed;
    return ResultType::ok;
}

ResultType restartClient(
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

    const ResultType result = sendCommandToApplauncher(
        StartApplicationTask(version, arguments),
        &response);
    return result != ResultType::ok ? result : response.result;
}

ResultType installZip(
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

ResultType installZipAsync(
    const nx::utils::SoftwareVersion& version,
    const QString& zipFileName)
{
    InstallZipTaskAsync request;
    request.version = version;
    request.zipFileName = zipFileName;
    Response response;

    ResultType result = ResultType::otherError;
    static const int kMaxTries = 5;

    for (int retries = 0; retries < kMaxTries; retries++)
    {
        result = sendCommandToApplauncher(request, &response);
        if (result == ResultType::ok)
            result = response.result;

        switch (result)
        {
            case ResultType::alreadyInstalled:
            case ResultType::otherError:
            case ResultType::versionNotInstalled:
            case ResultType::invalidVersionFormat:
            case ResultType::notEnoughSpace:
            case ResultType::notFound:
            case ResultType::busy:
            case ResultType::ok:
                return result;
            case ResultType::connectError:
            case ResultType::ioError:
            default:
                break;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return result;
}

ResultType checkInstallationProgress()
{
    InstallZipCheckStatus request;
    Response response;
    const auto result = sendCommandToApplauncher(request, &response);
    if (result != ResultType::ok)
        return result;
    if (response.result != ResultType::ok)
        return response.result;
    return response.result;
}

ResultType scheduleProcessKill(qint64 processId, quint32 timeoutMillis)
{
    AddProcessKillTimerRequest request;
    request.processId = processId;
    request.timeoutMillis = timeoutMillis;
    AddProcessKillTimerResponse response;
    ResultType result = sendCommandToApplauncher(request, &response);
    if (result != ResultType::ok)
        return result;
    return response.result;
}

ResultType quitApplauncher()
{
    QuitTask task;
    Response response;
    ResultType result = sendCommandToApplauncher(task, &response);
    if (result != ResultType::ok)
        return result;
    if (response.result != ResultType::ok)
        return response.result;
    return response.result;
}

ResultType getInstalledVersions(QList<nx::utils::SoftwareVersion>* versions)
{
    GetInstalledVersionsRequest request;
    GetInstalledVersionsResponse response;
    ResultType result = sendCommandToApplauncher(request, &response);
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

} // namespace nx::applauncher::api
