// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "applauncher_utils.h"

#include <thread>
#include <chrono>

#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDesktopWidget>

#include <common/common_module.h>

#include <client/client_startup_parameters.h>
#include <client/self_updater.h>

#include <nx/vms/applauncher/api/commands.h>

#include <nx/utils/log/log.h>

namespace nx::vms::applauncher::api {

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
    const QString& auth,
    const QString& systemUri)
{
    QStringList arguments;

    if(!systemUri.isEmpty())
    {
        arguments << "--" << systemUri;
    }
    else if (!auth.isEmpty())
    {
        arguments << "--auth" << auth;
    }

    arguments << "--no-single-application";

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

    int retries = 0;
    for (; retries < kMaxTries; retries++)
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

    if (retries == kMaxTries && result != ResultType::ok)
        NX_ERROR(NX_SCOPE_TAG, "installZipAsync got error %1 after %2 retries", result, retries);

    return result;
}

ResultType checkInstallationProgress(const nx::utils::SoftwareVersion& version,
    InstallationProgress& progress)
{
    InstallZipCheckStatus request;
    request.version = version;
    InstallZipCheckStatusResponse response;
    const auto result = sendCommandToApplauncher(request, &response);
    progress.total = response.total;
    progress.extracted = response.extracted;
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
    // New online checks, for applauncher with PingRequest
    static int lastPingRequest = 0;
    PingRequest request;
    request.pingId = lastPingRequest++;
    auto start = std::chrono::system_clock::now().time_since_epoch();
    request.pingStamp = std::chrono::duration_cast<std::chrono::milliseconds>(start).count();

    PingResponse response;
    ResultType result = sendCommandToApplauncher(request, &response);
    if (result == ResultType::ok)
    {
        auto finish = std::chrono::system_clock::now().time_since_epoch();
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
        NX_VERBOSE(NX_SCOPE_TAG, "checkOnline(runWhenOffline=%1) checked online status in %d ms",
            delta.count());
        return true;
    }
    else
    {
        // Legacy applauncher check, for the versions without PingRequest
        static const nx::utils::SoftwareVersion anyVersion(3, 0);
        bool notUsed = false;
        result = isVersionInstalled(anyVersion, &notUsed);
        if (result == ResultType::ok)
            return true;
    }

    return ((result == ResultType::connectError) && runWhenOffline
        && nx::vms::client::SelfUpdater::runMinilaucher());
}

} // namespace nx::vms::applauncher::api
