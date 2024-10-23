// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "applauncher_utils.h"

#include <chrono>
#include <thread>

#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <client/client_startup_parameters.h>
#include <client/self_updater.h>
#include <nx/utils/log/log.h>
#include <nx/vms/applauncher/api/commands.h>

namespace nx::vms::applauncher::api {

namespace {

static const int kZipInstallationTimeoutMs = 30000;

ResultType getApplauncherState()
{
    // Try to run applauncher if it is not running.
    static int lastPingRequest = 0;
    PingRequest request;
    request.pingId = ++lastPingRequest;
    auto start = std::chrono::system_clock::now().time_since_epoch();
    request.pingStamp = std::chrono::duration_cast<std::chrono::milliseconds>(start).count();

    PingResponse response;
    ResultType result = sendCommandToApplauncher(request, &response);

    if (result == ResultType::ok)
    {
        auto finish = std::chrono::system_clock::now().time_since_epoch();
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
        NX_DEBUG(NX_SCOPE_TAG, "Applauncher is online, checked in %1 ms.", delta.count());
        return result;
    }

    if (result == ResultType::connectError)
        NX_DEBUG(NX_SCOPE_TAG, "Applauncher is offline.");
    else
        NX_DEBUG(NX_SCOPE_TAG, "Error while sending command to applauncher, result: %1", result);

    return result;
}

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
    const QStringList& params,
    const QString& systemUri)
{
    QStringList arguments(params);

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

bool checkOnline()
{
    return getApplauncherState() == ResultType::ok;
}

bool checkOnlineTryToStartIfOffline()
{
    const auto result = getApplauncherState();

    if (result == ResultType::ok)
        return true;

    if (result != ResultType::connectError)
        return false;

    NX_DEBUG(NX_SCOPE_TAG, "Trying to start minilauncher.");

    return client::desktop::SelfUpdater::runMinilaucher();
}

} // namespace nx::vms::applauncher::api
