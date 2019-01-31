#pragma once

#include <nx/vms/applauncher/api/applauncher_api.h>

#include <nx/utils/software_version.h>

namespace applauncher {
namespace api {

//!Returns \a true if required version is installed and can be launched
/*!
    \note Application may be installed for all users and for current user only, so checking existance of directory with name equal to version is not enough
*/
ResultType::Value isVersionInstalled(
    nx::utils::SoftwareVersion version,
    bool* const installed);

//!Retrieves a list of the installed versions
ResultType::Value getInstalledVersions(QList<nx::utils::SoftwareVersion>* versions);
/*!
    Send the required version to the applauncher, close the current instance of the client.
    \return \a ResultType::ok if request has been performed successfully, otherwise - error code
*/
ResultType::Value restartClient(
    nx::utils::SoftwareVersion version = {},
    const QString& auth = {});

bool checkOnline(bool runWhenOffline = true);

//!Installs client update from a zip file
/*!
 * \param[in] version Version of the installation
 * \param[in] zipFileName Path to zip file
 * \return \a ResultType::ok if request has been performed successfully, otherwise - error code
 */
ResultType::Value installZip(
    const nx::utils::SoftwareVersion& version,
    const QString& zipFileName);

//!Adds timer to kill process with pid \a processID in a \a timeoutMillis
ResultType::Value scheduleProcessKill(qint64 processID, quint32 timeoutMillis);

//! Quits currently running applauncher if any
ResultType::Value quitApplauncher();

} // namespace api
} // namespace applauncher
