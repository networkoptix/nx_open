// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_error_strings.h"

#include <QtCore/QCoreApplication> //< For Q_DECLARE_TR_FUNCTIONS.

#include <nx/branding.h>
#include <nx/utils/software_version.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/network/server_compatibility_validator.h>

#include "remote_connection_error.h"

namespace nx::vms::client::core {

class RemoteConnectionErrorStrings
{
    Q_DECLARE_TR_FUNCTIONS(RemoteConnectionErrorStrings)

    static QString contactAdministrator()
    {
        return tr("If this error persists, please contact your system administrator.");
    }

    static QString clientCloudIsNotReady()
    {
        return tr("Connection to %1 is not ready yet. "
            "Check your internet connection or try again later.",
            "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName());
    }

    static QString serverCloudIsNotReady()
    {
        return tr("Connection to %1 is not ready yet. "
            "Check the server’s internet connection or try again later.",
            "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName());
    }

    static QString connectionError()
    {
        return tr("Connection to the server could not be established. Try again later.")
            + common::html::kLineBreak
            + contactAdministrator();
    }

public:
    static RemoteConnectionErrorDescription description(
        RemoteConnectionErrorCode code,
        const nx::vms::api::ModuleInformation& moduleInformation,
        const nx::utils::SoftwareVersion& engineVersion)
    {
        using namespace nx::vms::common;

        static const QString kRowMarker = " - ";
        const QString versionDetails =
            kRowMarker + tr("Client version: %1").arg(engineVersion.toString())
            + html::kLineBreak
            + kRowMarker + tr("Server version: %1.").arg(moduleInformation.version.toString());

        switch (code)
        {
            case RemoteConnectionErrorCode::binaryProtocolVersionDiffers:
            case RemoteConnectionErrorCode::cloudHostDiffers:
            {
                return {
                    tr("Incompatible server"),
                    tr("Server has a different version:")
                    + html::kLineBreak
                    + versionDetails
                    + html::kLineBreak
                    + tr("You will be asked to restart the client in compatibility mode.")
                };
            }

            case RemoteConnectionErrorCode::certificateRejected:
            {
                return {
                    tr("Invalid certificate"),
                    tr("The server&apos;s certificate was rejected.")
                };
            }

            case RemoteConnectionErrorCode::cloudSessionExpired:
            {
                return {
                    tr("Your session has expired"),
                    tr("Please log in to %1 again.", "%1 is the cloud name (like Nx Cloud)")
                        .arg(nx::branding::cloudName())
                };
            }

            case RemoteConnectionErrorCode::cloudUnavailableOnClient:
            {
                return {
                    tr("Restore the connection to %1 and try again",
                        "%1 is the cloud name (like Nx Cloud)")
                        .arg(nx::branding::cloudName()),
                    clientCloudIsNotReady() + html::kLineBreak + contactAdministrator()
                };
            }

            case RemoteConnectionErrorCode::cloudUnavailableOnServer:
            {
                return
                {
                    tr("%1 user login is temporarily unavailable",
                        "%1 is the short cloud name (like Cloud)")
                        .arg(nx::branding::shortCloudName()),
                    serverCloudIsNotReady() + html::kLineBreak + contactAdministrator()
                };
            }

            case RemoteConnectionErrorCode::connectionTimeout:
            {
                return {
                    tr("Connection failed"),
                    connectionError()
                };
            }

            case RemoteConnectionErrorCode::customizationDiffers:
            {
                return {
                    tr("Incompatible server"),
                    tr("The server is incompatible.")
                };
            }

            case RemoteConnectionErrorCode::factoryServer:
            {
                const QString message = tr("Connect to this server from web browser or through "
                    "desktop client to set it up");

                // This error message must be requested from the mobile client only.
                return {message, message};
            }

            case RemoteConnectionErrorCode::ldapInitializationInProgress:
            {
                return {
                    tr("LDAP Server connection timed out"),
                    tr("LDAP Server connection timed out.") + html::kLineBreak
                        + contactAdministrator()
                };
            }

            case RemoteConnectionErrorCode::loginAsCloudUserForbidden:
            {
                const QString message = tr("Log in to %1 to log in to this system with %2 user",
                    "%1 is the cloud name (like Nx Cloud), %2 is the short cloud name (like Cloud)")
                        .arg(html::localLink(nx::branding::cloudName(), "#cloud"), nx::branding::shortCloudName());

                return {message, message};
            }

            case RemoteConnectionErrorCode::sessionExpired:
            {
                return {
                    tr("Session expired. Re-enter your password."),
                    tr("Session duration limit can be changed by a system administrator.")
                };
            }

            case RemoteConnectionErrorCode::temporaryTokenExpired:
            {
                return {
                    tr("Your access to this system has expired."),
                    tr("Please contact the system administrator to regain access.")
                };
            }

            case RemoteConnectionErrorCode::systemIsNotCompatibleWith2Fa:
            {
                const QString message = tr("To log in to this System, disable “Ask for a "
                    "verification code on every login with your %1 account” in your %2.",
                    "%1 is the cloud name (like Nx Cloud),"
                    "%2 is link that leads to /account/security section of Nx Cloud")
                    .arg(nx::branding::cloudName(),
                        html::localLink("account settings", "#cloud_account_security"));

                return {message, message};
            }

            case RemoteConnectionErrorCode::twoFactorAuthOfCloudUserIsDisabled:
            {
                const QString message = tr("Two-factor authentication is required.")
                    + html::kLineBreak
                    + tr("You can enable two-factor authentication in your %1.",
                        "%1 is link that leads to /account/security section of Nx Cloud")
                          .arg(html::localLink("account settings", "#cloud_account_security"));

                return {message, message};
            }

            case RemoteConnectionErrorCode::unauthorized:
            {
                return {
                    tr("Invalid login or password"),
                    tr("Incorrect username or password.")
                };
            }

            case RemoteConnectionErrorCode::userIsDisabled:
            {
                return {
                    tr("User is disabled"),
                    tr("This user has been disabled by a system administrator.")
                };
            }

            case RemoteConnectionErrorCode::userIsLockedOut:
            {
                return {
                    tr("Too many attempts. Try again in a minute."),
                    tr("Too many login attempts. Try again in a minute.")
                        + html::kLineBreak
                        + contactAdministrator()
                };
            }

            case RemoteConnectionErrorCode::versionIsTooLow:
            {
                using nx::vms::common::ServerCompatibilityValidator;
                return {
                    tr("Incompatible server"),
                    tr("Server has a different version:")
                        + html::kLineBreak
                        + versionDetails
                        + html::kLineBreak
                        + tr("Compatibility mode for versions lower than %1 is not supported.")
                            .arg(ServerCompatibilityValidator::minimalVersion().toString())
                };
            }

            case RemoteConnectionErrorCode::forbiddenRequest:
            case RemoteConnectionErrorCode::genericNetworkError:
            case RemoteConnectionErrorCode::internalError:
            case RemoteConnectionErrorCode::networkContentError:
            case RemoteConnectionErrorCode::unexpectedServerId:
            default:
            {
                return {
                    tr("Internal error. Please try again later."),
                    connectionError()
                };
            }
        }
    }
};

RemoteConnectionErrorDescription errorDescription(
    RemoteConnectionErrorCode code,
    const nx::vms::api::ModuleInformation& moduleInformation,
    const nx::utils::SoftwareVersion& engineVersion)
{
    return RemoteConnectionErrorStrings::description(code, moduleInformation, engineVersion);
}

QString shortErrorDescription(RemoteConnectionErrorCode code)
{
    return RemoteConnectionErrorStrings::description(code, {}, {}).shortText;
}

} // namespace nx::vms::client::core
