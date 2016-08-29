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
        case Qn::ConnectionResult::success:
            return Qn::Empty_Help;
        case Qn::ConnectionResult::networkError:
        case Qn::ConnectionResult::unauthorized:
        case Qn::ConnectionResult::temporaryUnauthorized:
        case Qn::ConnectionResult::incompatibleInternal:
            return Qn::Login_Help;
        case Qn::ConnectionResult::incompatibleVersion:
        case Qn::ConnectionResult::compatibilityMode:
        case Qn::ConnectionResult::updateRequested:
            return Qn::VersionMismatch_Help;
    }
    NX_ASSERT(false, "Unhandled switch case");
    return Qn::Empty_Help;
}


}

QnConnectionDiagnosticsHelper::QnConnectionDiagnosticsHelper(QObject* parent):
    base_type(parent)
{

}

Qn::ConnectionResult QnConnectionDiagnosticsHelper::validateConnection(
    const QnConnectionInfo &connectionInfo,
    ec2::ErrorCode errorCode,
    const QUrl &url,
    QWidget* parentWidget)
{
    using namespace Qn;

    ConnectionResult result = base_type::validateConnection(connectionInfo, errorCode);
    if (result == ConnectionResult::success)
        return result;

    int helpTopicId = helpTopic(result);

    QString detail;
    if (result == ConnectionResult::unauthorized)
    {
        detail = tr("The username or password you have entered is incorrect. Please try again.");
    }
    else if (result == ConnectionResult::temporaryUnauthorized)
    {
        detail = tr("LDAP Server connection timed out.") + L'\n'
            + strings(ErrorStrings::ContactAdministrator);
    }
    else if (result == ConnectionResult::networkError)
    {
        detail = tr("Connection to the Server could not be established.") + L'\n'
            + tr("Connection details that you have entered are incorrect, please try again.") + L'\n'
            + strings(ErrorStrings::ContactAdministrator);
    }
    else if (result == ConnectionResult::incompatibleInternal)
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

    QString versionDetails =
        tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString()) + L'\n'
        + tr(" - Server version: %1.").arg(connectionInfo.version.toString()) + L'\n';

    if (result == ConnectionResult::incompatibleVersion)
    {
        QnMessageBox::warning(
            parentWidget,
            helpTopicId,
            strings(ErrorStrings::UnableConnect),
            tr("You are about to connect to Server which has a different version:") + L'\n'
            + versionDetails
            + tr("Compatibility mode for versions lower than %1 is not supported.")
            .arg(minSupportedVersion().toString()),
            QDialogButtonBox::Ok
        );
        return result;
    }

    bool haveExactVersion = false;
    bool needExactVersion = false;

    if (result == ConnectionResult::updateRequested)
    {
        needExactVersion = true;
        QList<QnSoftwareVersion> versions;
        if (applauncher::getInstalledVersions(&versions) == applauncher::api::ResultType::ok)
        {
            haveExactVersion = versions.contains(connectionInfo.version);
        }
        else
        {
            QString olderComponent = connectionInfo.nxClusterProtoVersion < QnAppInfo::ec2ProtoVersion()
                ? tr("Server")
                : tr("Client");
            QString message = tr("You are about to connect to Server which has a different version:") + L'\n'
                + versionDetails
                + tr("These versions are not compatible. Please update your %1.").arg(olderComponent);
#ifdef _DEBUG
            message += lit("\nClient Proto: %1\nServer Proto: %2").arg(QnAppInfo::ec2ProtoVersion()).arg(connectionInfo.nxClusterProtoVersion);
#endif
            QnMessageBox::warning(
                parentWidget,
                helpTopicId,
                strings(ErrorStrings::UnableConnect),
                message,
                QDialogButtonBox::Ok
            );
            return result;
        }
    }

    NX_ASSERT(result == ConnectionResult::compatibilityMode);

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
            return ConnectionResult::incompatibleVersion;
        }

        if (!isInstalled || (needExactVersion && !haveExactVersion))
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
            return ConnectionResult::incompatibleVersion;
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
            return ConnectionResult::incompatibleVersion;

        switch (applauncher::restartClient(connectionInfo.version, url.toEncoded()))
        {
            case applauncher::api::ResultType::ok:
                return ConnectionResult::compatibilityMode;

            case applauncher::api::ResultType::connectError:
                QnMessageBox::critical(
                    parentWidget,
                    tr("Launcher process not found."),
                    tr("Cannot restart the Client in compatibility mode.") + L'\n'
                    + tr("Please close the application and start it again using the shortcut in the start menu.")
                );
                return ConnectionResult::incompatibleVersion;

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
                return ConnectionResult::incompatibleVersion;
            }
        } // switch restartClient

    } // while(true)

    NX_ASSERT(false);    //should never get here
    return ConnectionResult::incompatibleVersion; //just in case
}

QnConnectionDiagnosticsHelper::TestConnectionResult QnConnectionDiagnosticsHelper::validateConnectionTest(
    const QnConnectionInfo& connectionInfo,
    ec2::ErrorCode errorCode)
{
    using namespace Qn;
    TestConnectionResult result;

    //TODO #GDM almost same code exists in QnConnectionDiagnosticsHelper::validateConnection

    result.result = base_type::validateConnection(connectionInfo, errorCode);
    result.helpTopicId = helpTopic(result.result);

    QString versionDetails =
        tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString()) + L'\n'
        + tr(" - Server version: %1.").arg(connectionInfo.version.toString()) + L'\n';

    switch (result.result)
    {
        case ConnectionResult::success:
            break;
        case ConnectionResult::unauthorized:
        {
            result.details = tr("The username or password you have entered is incorrect. Please try again.");
            break;
        }
        case ConnectionResult::temporaryUnauthorized:
        {
            result.details = tr("LDAP Server connection timed out.") + L'\n'
                + strings(ErrorStrings::ContactAdministrator);
            break;
        }
        case ConnectionResult::networkError:
        {
            result.details = tr("Connection to the Server could not be established.") + L'\n'
                + tr("Connection details that you have entered are incorrect, please try again.") + L'\n'
                + strings(ErrorStrings::ContactAdministrator);
            break;
        }
        case ConnectionResult::incompatibleInternal:
        {
            result.details = tr("You are trying to connect to incompatible Server.");
            break;
        }
        case ConnectionResult::incompatibleVersion:
        {
            result.details = tr("Server has a different version:") + L'\n'
                + versionDetails
                + tr("Compatibility mode for versions lower than %1 is not supported.")
                .arg(minSupportedVersion().toString());
            break;
        }
        case ConnectionResult::compatibilityMode:
        {
            result.details = tr("Server has a different version:") + L'\n'
                + versionDetails
                + tr("You will be asked to restart the client in compatibility mode.");
            break;
        }
        case ConnectionResult::updateRequested:
        {
            QString olderComponent = connectionInfo.nxClusterProtoVersion < QnAppInfo::ec2ProtoVersion()
                ? tr("Server")
                : tr("Client");
            result.details = tr("Server has a different version:") + L'\n'
                + versionDetails
                + tr("You will be asked to update your %1").arg(olderComponent);
            break;
        }
        default:
            break;
    }
    return result;
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

