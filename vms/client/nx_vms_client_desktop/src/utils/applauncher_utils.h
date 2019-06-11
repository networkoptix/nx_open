#pragma once

#include <nx/vms/applauncher/api/applauncher_api.h>

#include <nx/utils/software_version.h>
#include <nx/vms/api/data/software_version.h>

namespace nx::vms::applauncher::api {

/**
 * @returns true if required version is installed and can be launched.
 * Note application may be installed for all users and for current user only, so checking existance
 * of directory with name equal to version is not enough.
*/
ResultType isVersionInstalled(const nx::utils::SoftwareVersion& version, bool* const installed);

/** Retrieves a list of the installed versions. */
ResultType getInstalledVersions(QList<nx::utils::SoftwareVersion>* versions);

/**
 * Send the required version to the applauncher, close the current instance of the client.
 *
 * @return ResultType::ok if request has been performed successfully, otherwise - error code.
 */
ResultType restartClient(nx::utils::SoftwareVersion version = {}, const QString& auth = {});

bool checkOnline(bool runWhenOffline = true);

/**
 * Installs client update from a zip file.
 * @param version Version of the installation.
 * @param zipFileName Path to zip file.
 * @return ResultType::ok if request has been performed successfully, otherwise - error code.
 */
ResultType installZip(const nx::utils::SoftwareVersion& version, const QString& zipFileName);

/**
 * Installs client update from a zip file. It will run installation asynchronously.
 * @param version Version of the installation.
 * @param zipFileName Path to zip file.
 * @return ResultType::ok if request has been performed successfully, otherwise - error code.
 */
ResultType installZipAsync(const nx::utils::SoftwareVersion& version, const QString& zipFileName);


struct InstallationProgress
{
    uint64_t extracted = 0;
    uint64_t total = 0;
};
/**
 * Checks progress of an installation.
 * @return ResultType::ok if request has been performed successfully, otherwise - error code.
 */
ResultType checkInstallationProgress(InstallationProgress& progress);

/** Adds timer to kill process with pid processId in a timeoutMillis. */
ResultType scheduleProcessKill(qint64 processId, quint32 timeoutMillis);

/** Quits currently running applauncher if any. */
ResultType quitApplauncher();

} // namespace nx::vms::applauncher::api
