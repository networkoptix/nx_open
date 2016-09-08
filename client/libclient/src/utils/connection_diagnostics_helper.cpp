#include "connection_diagnostics_helper.h"

#include <api/model/connection_info.h>

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <nx_ec/ec_api.h>

#include <network/module_information.h>

#include <ui/dialogs/compatibility_version_installation_dialog.h>
#include <ui/help/help_topics.h>

#include <utils/applauncher_utils.h>
#include <utils/common/software_version.h>
#include <utils/common/app_info.h>

namespace {

Qn::HelpTopic helpTopic(Qn::ConnectionResult result)
{
    switch (result)
    {
        case Qn::ConnectionResult::Success:
            return Qn::Empty_Help;
        case Qn::ConnectionResult::NetworkError:
        case Qn::ConnectionResult::Unauthorized:
        case Qn::ConnectionResult::TemporaryUnauthorized:
        case Qn::ConnectionResult::IncompatibleInternal:
            return Qn::Login_Help;
        case Qn::ConnectionResult::IncompatibleVersion:
        case Qn::ConnectionResult::IncompatibleProtocol:
            return Qn::VersionMismatch_Help;
    }
    NX_ASSERT(false, "Unhandled switch case");
    return Qn::Empty_Help;
}

} // namespace

QnConnectionDiagnosticsHelper::QnConnectionDiagnosticsHelper(QObject* parent):
    base_type(parent)
{
}

Qn::ConnectionResult QnConnectionDiagnosticsHelper::validateConnection(
    const QnConnectionInfo &connectionInfo,
    ec2::ErrorCode errorCode,
    QWidget* parentWidget)
{
    using namespace Qn;

    ConnectionResult result = QnConnectionValidator::validateConnection(connectionInfo, errorCode);
    if (result == ConnectionResult::Success)
        return result;

    int helpTopicId = helpTopic(result);

    QString detail;
    if (result == ConnectionResult::Unauthorized)
    {
        detail = tr("The username or password you have entered is incorrect. Please try again.");
    }
    else if (result == ConnectionResult::TemporaryUnauthorized)
    {
        detail = tr("LDAP Server connection timed out.") + L'\n'
            + strings(ErrorStrings::ContactAdministrator);
    }
    else if (result == ConnectionResult::NetworkError)
    {
        detail = tr("Connection to the Server could not be established.") + L'\n'
            + tr("Connection details that you have entered are incorrect, please try again.") + L'\n'
            + strings(ErrorStrings::ContactAdministrator);
    }
    else if (result == ConnectionResult::IncompatibleInternal)
    {
        detail = tr("You are trying to connect to incompatible Server.");
    }

    if (!detail.isEmpty())
    {
        QnMessageBox::warning(
            parentWidget,
            helpTopicId,
            strings(ErrorStrings::UnableConnect),
            detail
        );
        return result;
    }

    const auto versionDetails =
        tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString()) + L'\n'
        + tr(" - Server version: %1.").arg(connectionInfo.version.toString()) + L'\n';

    if (result == ConnectionResult::IncompatibleVersion)
    {
        QnMessageBox::warning(
            parentWidget,
            helpTopicId,
            strings(ErrorStrings::UnableConnect),
            tr("You are about to connect to Server which has a different version:") + L'\n'
            + versionDetails
            + tr("Compatibility mode for versions lower than %1 is not supported.")
            .arg(QnConnectionValidator::minSupportedVersion().toString()),
            QDialogButtonBox::Ok
        );
        return result;
    }

    if (result == ConnectionResult::IncompatibleProtocol)
        return handleCompatibilityMode(connectionInfo, parentWidget);

    NX_ASSERT(false);    //should never get here
    return ConnectionResult::IncompatibleVersion; //just in case
}

QnConnectionDiagnosticsHelper::TestConnectionResult QnConnectionDiagnosticsHelper::validateConnectionTest(
    const QnConnectionInfo& connectionInfo,
    ec2::ErrorCode errorCode)
{
    using namespace Qn;
    TestConnectionResult result;

    //TODO #GDM almost same code exists in QnConnectionDiagnosticsHelper::validateConnection

    result.result = QnConnectionValidator::validateConnection(connectionInfo, errorCode);
    result.helpTopicId = helpTopic(result.result);

    QString versionDetails =
        tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString()) + L'\n'
        + tr(" - Server version: %1.").arg(connectionInfo.version.toString()) + L'\n';

    switch (result.result)
    {
        case ConnectionResult::Success:
            break;
        case ConnectionResult::Unauthorized:
        {
            result.details = tr("The username or password you have entered is incorrect. Please try again.");
            break;
        }
        case ConnectionResult::TemporaryUnauthorized:
        {
            result.details = tr("LDAP Server connection timed out.") + L'\n'
                + strings(ErrorStrings::ContactAdministrator);
            break;
        }
        case ConnectionResult::NetworkError:
        {
            result.details = tr("Connection to the Server could not be established.") + L'\n'
                + tr("Connection details that you have entered are incorrect, please try again.") + L'\n'
                + strings(ErrorStrings::ContactAdministrator);
            break;
        }
        case ConnectionResult::IncompatibleInternal:
        {
            result.details = tr("You are trying to connect to incompatible Server.");
            break;
        }
        case ConnectionResult::IncompatibleVersion:
        {
            result.details = tr("Server has a different version:") + L'\n'
                + versionDetails
                + tr("Compatibility mode for versions lower than %1 is not supported.")
                .arg(QnConnectionValidator::minSupportedVersion().toString());
            break;
        }
        case ConnectionResult::IncompatibleProtocol:
        {
            result.details = tr("Server has a different version:") + L'\n'
                + versionDetails
                + tr("You will be asked to restart the client in compatibility mode.");
            break;
        }
        default:
            break;
    }
    return result;
}

Qn::ConnectionResult QnConnectionDiagnosticsHelper::handleCompatibilityMode(
    const QnConnectionInfo &connectionInfo,
    QWidget* parentWidget)
{
    using namespace Qn;
    int helpTopicId = helpTopic(ConnectionResult::IncompatibleProtocol);

    const auto versionDetails =
        tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString())
        + L'\n'
        + tr(" - Server version: %1.").arg(connectionInfo.version.toString())
        + L'\n';

    bool haveExactVersion = false;
    QList<QnSoftwareVersion> versions;
    if (applauncher::getInstalledVersions(&versions) == applauncher::api::ResultType::ok)
        haveExactVersion = versions.contains(connectionInfo.version);

    while (true)
    {
        bool isInstalled = false;
        if (applauncher::isVersionInstalled(connectionInfo.version, &isInstalled) != applauncher::api::ResultType::ok)
        {
            QnMessageBox::warning(
                parentWidget,
                helpTopicId,
                strings(ErrorStrings::UnableConnect),
                tr("Selected Server has a different version:") + L'\n'
                + versionDetails
                + tr("An error has occurred while trying to restart in compatibility mode."),
                QDialogButtonBox::Ok
            );
            return ConnectionResult::IncompatibleVersion;
        }

        if (!isInstalled || !haveExactVersion)
        {
            QString versionString = connectionInfo.version.toString(
                CompatibilityVersionInstallationDialog::useUpdate(connectionInfo.version)
                ? QnSoftwareVersion::FullFormat
                : QnSoftwareVersion::MinorFormat);

            int selectedButton = QnMessageBox::warning(
                parentWidget,
                helpTopicId,
                strings(ErrorStrings::UnableConnect),
                tr("You are about to connect to Server which has a different version:") + L'\n'
                + tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString()) + L'\n'
                + tr(" - Server version: %1.").arg(versionString) + L'\n'
                + tr("Client version %1 is required to connect to this Server.").arg(versionString) + L'\n'
                + tr("Download version %1?").arg(versionString),

                QDialogButtonBox::StandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::Cancel),
                QDialogButtonBox::Cancel
            );

            if (selectedButton == QDialogButtonBox::Yes)
            {
                QScopedPointer<CompatibilityVersionInstallationDialog> installationDialog(
                    new CompatibilityVersionInstallationDialog(connectionInfo.version, parentWidget));
                //starting installation
                installationDialog->exec();
                if (installationDialog->installationSucceeded())
                {
                    haveExactVersion = true;
                    continue;   //offering to start newly-installed compatibility version
                }
            }
            return ConnectionResult::IncompatibleVersion;
        }

        //version is installed, trying to run
        int button = QnMessageBox::warning(
            parentWidget,
            helpTopicId,
            strings(ErrorStrings::UnableConnect),
            tr("You are about to connect to Server which has a different version:") + L'\n'
            + versionDetails
            + tr("Would you like to restart the Client in compatibility mode?"),
            QDialogButtonBox::StandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel),
            QDialogButtonBox::Cancel
        );

        if (button != QDialogButtonBox::Ok)
            return ConnectionResult::IncompatibleVersion;

        switch (applauncher::restartClient(connectionInfo.version, connectionInfo.ecUrl.toEncoded()))
        {
            case applauncher::api::ResultType::ok:
                return ConnectionResult::IncompatibleProtocol;

            case applauncher::api::ResultType::connectError:
                QnMessageBox::critical(
                    parentWidget,
                    tr("Launcher process not found."),
                    tr("Cannot restart the Client in compatibility mode.") + L'\n'
                    + tr("Please close the application and start it again using the shortcut in the start menu.")
                );
                return ConnectionResult::IncompatibleVersion;

            default:
            {
                //trying to restore installation
                int selectedButton = QnMessageBox::warning(
                    parentWidget,
                    helpTopicId,
                    tr("Failure"),
                    tr("Failed to launch compatibility version %1").arg(connectionInfo.version.toString(QnSoftwareVersion::MinorFormat)) + L'\n'
                    + tr("Try to restore version %1?").arg(connectionInfo.version.toString(QnSoftwareVersion::MinorFormat)),
                    QDialogButtonBox::StandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel),
                    QDialogButtonBox::Cancel
                );
                if (selectedButton == QDialogButtonBox::Ok)
                {
                    //starting installation
                    QScopedPointer<CompatibilityVersionInstallationDialog> installationDialog(new CompatibilityVersionInstallationDialog(connectionInfo.version, parentWidget));
                    installationDialog->exec();
                    if (installationDialog->installationSucceeded())
                        continue;   //offering to start newly-installed compatibility version
                }
                return ConnectionResult::IncompatibleVersion;
            }
        } // switch restartClient

    } // while(true)

    /* Just in case, should never get here. */
    NX_ASSERT(false, "Should never get here");
    return ConnectionResult::IncompatibleVersion;
}

QString QnConnectionDiagnosticsHelper::strings(ErrorStrings id)
{
    switch (id)
    {
        case ErrorStrings::ContactAdministrator:
            return tr("If this error persists, please contact your VMS administrator.");
        case ErrorStrings::UnableConnect:
            return tr("Unable to connect to the server");
        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Should never get here");
            break;
    }
    return QString();
}
