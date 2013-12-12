#ifndef APPLAUNCHER_UTILS_H
#define APPLAUNCHER_UTILS_H

#include <connectinfo.h>
#include <api/applauncher_api.h>
#include <utils/common/software_version.h>


namespace applauncher
{
    //bool canRestart(QnSoftwareVersion version = QnSoftwareVersion());

    //!Returns \a true if required version is installed and can be launched
    /*!
        \note Application may be installed for all users and for current user only, so checking existance of directory with name equal to version is not enough
    */
    api::ResultType::Value isVersionInstalled( QnSoftwareVersion version, bool* const installed );

    /*!
        Send the required version to the applauncher, close the current instance of the client.
        \return \a ResultType::ok if request has been performed successfully, otherwise - error code
    */
    api::ResultType::Value restartClient(QnSoftwareVersion version = QnSoftwareVersion(),
                       const QByteArray &auth = QByteArray());

    //!Starts installation of required version
    /*!
        \param[out] installationID Unique ID of installation is returned. This id can be used to request installation status
        \return \a ResultType::ok if installation has been started successfully, otherwise - error code
    */
    api::ResultType::Value startInstallation(
        const QnSoftwareVersion& version,
        unsigned int* installationID );
    //!Check running installation status
    /*!
        \param[in] installationID ID returned by \a applauncher::startInstallation()
        \param[out] status Status of installation
        \param[out] progress Installation progress (percent)
        \return \a ResultType::ok if request has been performed successfully, otherwise - error code
        \note If \a installationID is not a valid installation ID, then \a status is set to api::InstallationStatus::unknown
        \note \a installationID is forgotten after this function returned status \a api::InstallationStatus::success or \a api::InstallationStatus::failed
        \note if function failed, output arguments are not modified
    */
    api::ResultType::Value getInstallationStatus(
        unsigned int installationID,
        api::InstallationStatus::Value* const status,
        float* progress );
    //!Cancels running installation, removing already installed files
    /*!
        If installation have already finished successfully, nothing is done
        \param[in] installationID ID returned by \a applauncher::startInstallation()
        \return \a ResultType::ok if request has been performed successfully, otherwise - error code
    */
    api::ResultType::Value cancelInstallation( unsigned int installationID );
}

#endif // APPLAUNCHER_UTILS_H
