#include "connection_diagnostics_helper.h"

#include <QtCore/QThread>

#include <api/model/connection_info.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/self_updater.h>
#include <client/client_app_info.h>
#include <client/client_startup_parameters.h>

#include <nx/vms/client/desktop/ini.h>
#include <nx_ec/ec_api.h>

#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_topics.h>

#include <utils/applauncher_utils.h>
#include <utils/common/app_info.h>

#include <nx/vms/client/desktop/system_update/compatibility_version_installation_dialog.h>
#include <nx/network/http/http_types.h>
#include <nx/network/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/vms/api/protocol_version.h>

using namespace nx::vms::client::desktop;

namespace {

Qn::HelpTopic helpTopic(Qn::ConnectionResult result)
{
    switch (result)
    {
        case Qn::SuccessConnectionResult:
            return Qn::Empty_Help;
        case Qn::NetworkErrorConnectionResult:
        case Qn::UnauthorizedConnectionResult:
        case Qn::LdapTemporaryUnauthorizedConnectionResult:
        case Qn::CloudTemporaryUnauthorizedConnectionResult:
        case Qn::IncompatibleInternalConnectionResult:
        case Qn::ForbiddenConnectionResult:
        case Qn::DisabledUserConnectionResult:
        case Qn::UserTemporaryLockedOut:
            return Qn::Login_Help;
        case Qn::IncompatibleCloudHostConnectionResult:
        case Qn::IncompatibleVersionConnectionResult:
        case Qn::IncompatibleProtocolConnectionResult:
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

QString QnConnectionDiagnosticsHelper::getErrorDescription(
    Qn::ConnectionResult result,
    const QnConnectionInfo& connectionInfo,
    const nx::vms::api::SoftwareVersion& engineVersion)
{
    static const QString kRowMarker = lit(" - ");
    QString versionDetails;

    auto addRow = [&versionDetails](const QString& text)
        {
            versionDetails += kRowMarker + text + L'\n';
        };


    addRow(tr("Client version: %1.").arg(engineVersion.toString()));

    if (ini().developerMode)
    {
        addRow(lit("Protocol: %1, Cloud: %2")
            .arg(nx::vms::api::protocolVersion())
            .arg(nx::network::SocketGlobals::cloud().cloudHost()));
    }

    addRow(tr("Server version: %1.").arg(connectionInfo.version.toString()));

    if (ini().developerMode)
    {
        addRow(lit("Brand: %1, Customization: %2")
            .arg(connectionInfo.brand)
            .arg(connectionInfo.customization));
        addRow(lit("Protocol: %1, Cloud: %2")
            .arg(connectionInfo.nxClusterProtoVersion)
            .arg(connectionInfo.cloudHost));
    }

    switch (result)
    {
    case Qn::SuccessConnectionResult:
        return QString();
    case Qn::UnauthorizedConnectionResult:
        return tr("The username or password you have entered is incorrect. Please try again.");
    case Qn::LdapTemporaryUnauthorizedConnectionResult:
        return tr("LDAP Server connection timed out.") + L'\n'
            + getErrorString(ErrorStrings::ContactAdministrator);
    case Qn::CloudTemporaryUnauthorizedConnectionResult:
        return getErrorString(ErrorStrings::CloudIsNotReady)
            + L'\n' + getErrorString(ErrorStrings::ContactAdministrator);
    case Qn::DisabledUserConnectionResult:
        return tr("This user is disabled by system administrator.");
    case Qn::ForbiddenConnectionResult:
        return tr("Operation is not permitted now. It could happen due to server is restarting now. Please try again later.")
            + L'\n' + getErrorString(ErrorStrings::ContactAdministrator);
    case Qn::NetworkErrorConnectionResult:
        return tr("Connection to Server could not be established.") + L'\n'
            + tr("Connection details that you have entered are incorrect, please try again.") + L'\n'
            + getErrorString(ErrorStrings::ContactAdministrator);
    case Qn::IncompatibleInternalConnectionResult:
        return tr("You are trying to connect to incompatible Server.");
    case Qn::IncompatibleVersionConnectionResult:
    {
        return tr("Server has a different version:") + L'\n'
            + versionDetails
            + tr("Compatibility mode for versions lower than %1 is not supported.")
            .arg(QnConnectionValidator::minSupportedVersion().toString());
    }
    case Qn::IncompatibleCloudHostConnectionResult:
    case Qn::IncompatibleProtocolConnectionResult:
        return tr("Server has a different version:") + L'\n'
            + versionDetails
            + tr("You will be asked to restart the client in compatibility mode.");
        break;
    default:
        return QString();
    }
}

Qn::ConnectionResult QnConnectionDiagnosticsHelper::validateConnection(
    const QnConnectionInfo& connectionInfo,
    ec2::ErrorCode errorCode,
    QWidget* parentWidget,
    const nx::vms::api::SoftwareVersion& engineVersion)
{
    const auto result = QnConnectionValidator::validateConnection(connectionInfo, errorCode);
    if (result == Qn::SuccessConnectionResult)
    {
        // Forcing compatibility mode for debug purposes.
        if (ini().forceCompatibilityMode)
            return handleCompatibilityMode(connectionInfo, parentWidget, engineVersion);

        return result;
    }

    if (result == Qn::IncompatibleProtocolConnectionResult
        || result == Qn::IncompatibleCloudHostConnectionResult)
    {
        return handleCompatibilityMode(connectionInfo, parentWidget, engineVersion);
    }

    showValidateConnectionErrorMessage(parentWidget,
        result, connectionInfo, engineVersion);
    return result;
}

QString QnConnectionDiagnosticsHelper::ldapServerTimeoutMessage()
{
    return tr("LDAP Server connection timed out.");
}

void QnConnectionDiagnosticsHelper::showValidateConnectionErrorMessage(
    QWidget* parentWidget,
    Qn::ConnectionResult result,
    const QnConnectionInfo& connectionInfo,
    const nx::vms::api::SoftwareVersion& engineVersion)
{
    const QString serverVersion = connectionInfo.version.toString();

    static const auto kFailedToConnectText = tr("Failed to connect to Server");

    switch (result)
    {
        case Qn::UnauthorizedConnectionResult:
            QnMessageBox::warning(parentWidget, tr("Incorrect username or password"));
            break;
        case Qn::LdapTemporaryUnauthorizedConnectionResult:
            QnMessageBox::critical(parentWidget,
                kFailedToConnectText,
                ldapServerTimeoutMessage() + L'\n'
                    + getErrorString(ErrorStrings::ContactAdministrator));
            break;
        case Qn::CloudTemporaryUnauthorizedConnectionResult:
            QnMessageBox::critical(parentWidget,
                kFailedToConnectText,
                getErrorString(ErrorStrings::CloudIsNotReady)
                    + L'\n' + getErrorString(ErrorStrings::ContactAdministrator));
            break;
        case Qn::ForbiddenConnectionResult:
            QnMessageBox::warning(parentWidget,
                kFailedToConnectText,
                tr("Server may be restarting now. Please try again later.")
                    + L'\n' + getErrorString(ErrorStrings::ContactAdministrator));
            break;
        case Qn::DisabledUserConnectionResult:
            QnMessageBox::warning(parentWidget, tr("This user is disabled by system administrator."));
            break;
        case Qn::NetworkErrorConnectionResult:
            QnMessageBox::critical(parentWidget,
                kFailedToConnectText,
                tr("Please check access credentials and try again.")
                    + L'\n' + getErrorString(ErrorStrings::ContactAdministrator));
            break;
        case Qn::IncompatibleInternalConnectionResult:
        {
            QString message = tr("Incompatible Server");
            if (ini().developerMode)
                message += L'\n' + lit("Protocol: %1").arg(connectionInfo.nxClusterProtoVersion);
            QnMessageBox::warning(parentWidget, message);
            break;
        }
        case Qn::IncompatibleCloudHostConnectionResult:
        {
            QString message = tr("Incompatible Server");
            if (ini().developerMode)
                message += L'\n' + lit("Cloud host: %1").arg(connectionInfo.cloudHost);
            QnMessageBox::warning(parentWidget, message);
            break;
        }
        case Qn::IncompatibleVersionConnectionResult:
            QnMessageBox::critical(parentWidget,
                getDiffVersionsText(),
                getDiffVersionsExtra(engineVersion.toString(), serverVersion) + L'\n'
                    + tr("Compatibility mode for versions lower than %1 is not supported.")
                        .arg(QnConnectionValidator::minSupportedVersion().toString()));
            break;
        case Qn::IncompatibleProtocolConnectionResult:
            QnMessageBox::warning(parentWidget,
                QString(),
                getDiffVersionsFullText(engineVersion.toString(), serverVersion)
                    + L'\n' + tr("Restart %1 in compatibility mode "
                        "will be required.").arg(QnClientAppInfo::applicationDisplayName()));
            break;
        case Qn::UserTemporaryLockedOut:
        {
            QString message = tr("Too many attempts. Try again in a minute.");
            QnMessageBox::critical(parentWidget, message);
            break;
        }
        default:
            break;
    }
}


QnConnectionDiagnosticsHelper::TestConnectionResult
QnConnectionDiagnosticsHelper::validateConnectionTest(
    const QnConnectionInfo& connectionInfo,
    ec2::ErrorCode errorCode,
    const nx::vms::api::SoftwareVersion& engineVersion)
{
    using namespace Qn;
    TestConnectionResult result;

    //TODO #GDM almost same code exists in QnConnectionDiagnosticsHelper::validateConnection

    result.result = QnConnectionValidator::validateConnection(connectionInfo, errorCode);
    result.helpTopicId = helpTopic(result.result);

    result.details = getErrorDescription(result.result, connectionInfo, engineVersion);
    return result;
}

bool QnConnectionDiagnosticsHelper::getInstalledVersions(
    QList<nx::utils::SoftwareVersion>* versions)
{
    using nx::vms::applauncher::api::ResultType;

    /* Try to run applauncher if it is not running. */
    if (!nx::vms::applauncher::api::checkOnline())
        return false;

    const auto result = nx::vms::applauncher::api::getInstalledVersions(versions);
    if (result == ResultType::ok)
        return true;

    static const int kMaxTries = 5;
    for (int i = 0; i < kMaxTries; ++i)
    {
        QThread::msleep(100);
        qApp->processEvents();
        if (nx::vms::applauncher::api::getInstalledVersions(versions) == ResultType::ok)
            return true;
    }
    return false;
}

QString QnConnectionDiagnosticsHelper::getDiffVersionFullExtras(
    const QnConnectionInfo& serverInfo,
    const QString& extraText,
    const nx::vms::api::SoftwareVersion& engineVersion)
{
    const QString clientVersion = engineVersion.toString();
    const QString serverVersion = serverInfo.version.toString();

    QString developerModeText;
    if (ini().developerMode)
    {
        developerModeText =
            "\n<p style=\"text-indent: 0; font-size: 8px; color: #CCC;\">Developer info:<br/>";

        const bool protocolDiffers =
            nx::vms::api::protocolVersion() != serverInfo.nxClusterProtoVersion;
        if (protocolDiffers)
        {
            developerModeText +=
                QString("Protocol differs: <b>%1</b> on client, <b>%2</b> on server<br/>")
                    .arg(nx::vms::api::protocolVersion())
                    .arg(serverInfo.nxClusterProtoVersion);
        }

        const bool cloudHostDiffers =
            nx::network::SocketGlobals::cloud().cloudHost() != serverInfo.cloudHost;
        if (cloudHostDiffers)
        {
            developerModeText +=
                QString("Cloud Host differs: <b>%1</b> on client, <b>%2</b> on server<br/>")
                .arg(nx::network::SocketGlobals::cloud().cloudHost())
                .arg(serverInfo.cloudHost);
        }

        developerModeText += "</p>";
    }

    return getDiffVersionsFullText(clientVersion, serverVersion)
        + "\n" + extraText + developerModeText;
}

QString QnConnectionDiagnosticsHelper::getDiffVersionsText()
{
    return tr("Client and Server have different versions");
}

QString QnConnectionDiagnosticsHelper::getDiffVersionsExtra(
    const QString& clientVersion,
    const QString& serverVersion)
{
    return tr("Client - %1", "%1 is version").arg(clientVersion)
        + "<br/>" + tr("Server - %1", "%1 is version").arg(serverVersion);
}

QString QnConnectionDiagnosticsHelper::getDiffVersionsFullText(
    const QString& clientVersion,
    const QString& serverVersion)
{
    return getDiffVersionsText() + ":<br/>"
        + getDiffVersionsExtra(clientVersion, serverVersion);
}

Qn::ConnectionResult QnConnectionDiagnosticsHelper::handleCompatibilityMode(
    const QnConnectionInfo &connectionInfo,
    QWidget* parentWidget,
    const nx::vms::api::SoftwareVersion& engineVersion)
{
    auto handleApplauncherError =
        [&]()
        {
            auto extras = getDiffVersionFullExtras(connectionInfo,
                tr("Please close %1 and start it again using the shortcut in the start menu.")
                .arg(QnClientAppInfo::applicationDisplayName()),
                engineVersion);

            QnMessageBox::critical(parentWidget,
                tr("Failed to restart %1 in compatibility mode")
                .arg(QnClientAppInfo::applicationDisplayName()),
                extras);

            return Qn::IncompatibleVersionConnectionResult;
        };


    using namespace Qn;
    using namespace nx::vms::applauncher::api;

    using Dialog = CompatibilityVersionInstallationDialog;
    QList<nx::utils::SoftwareVersion> versions;
    if (!getInstalledVersions(&versions))
        return handleApplauncherError();

    QList<QString> versionStrings;
    for (auto version: versions)
        versionStrings.append(version.toString());
    NX_VERBOSE(NX_SCOPE_TAG,
        "handleCompatibilityMode() - have the following versions installed: %1", versionStrings);
    bool isInstalled = versions.contains(connectionInfo.version);
    bool shouldAutoRestart = false;

    while (true)
    {
        QString versionString = connectionInfo.version.toString();

        if (!isInstalled)
        {
            auto extras = getDiffVersionFullExtras(connectionInfo,
                tr("You have to download another version of %1 to "
                    "connect to this Server.").arg(QnClientAppInfo::applicationDisplayName()),
                engineVersion);

            QnMessageBox dialog(QnMessageBoxIcon::Question,
                tr("Download Client version %1?").arg(versionString), extras,
                QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, parentWidget);

            dialog.addButton(tr("Download && Install"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);

            if (dialog.exec() == QDialogButtonBox::Cancel)
                return Qn::IncompatibleVersionConnectionResult;

            QScopedPointer<Dialog> installationDialog(new Dialog(
                connectionInfo, parentWidget, engineVersion));

            //starting installation
            if (installationDialog->exec() == QDialog::Rejected)
                return Qn::IncompatibleVersionConnectionResult;
            // Possible scenarios: cancel, failed, success
            auto result = installationDialog->installationResult();
            if (result == Dialog::InstallResult::complete)
            {
                isInstalled = true;
                shouldAutoRestart = installationDialog->shouldAutoRestart();
            }
            else
            {
                QString errorString = installationDialog->errorString();
                if (!errorString.isEmpty())
                    errorString = QString("%1\n%2").arg(errorString, extras);
                else
                    errorString = extras;
                QnMessageBox dialog(QnMessageBoxIcon::Critical,
                    tr("Failed to enter compatibility mode for version %1").arg(versionString), errorString,
                    QDialogButtonBox::Ok, QDialogButtonBox::NoButton, parentWidget);
                dialog.exec();
                return Qn::IncompatibleVersionConnectionResult;
            }
        }

        //version is installed, trying to run
        const auto extras = getDiffVersionFullExtras(connectionInfo,
            tr("You have to restart %1 in compatibility"
                " mode to connect to this Server.").arg(QnClientAppInfo::applicationDisplayName()),
            engineVersion);

        if (!shouldAutoRestart)
        {
            QnMessageBox dialog(QnMessageBoxIcon::Question,
                tr("Restart %1 in compatibility mode?").arg(QnClientAppInfo::applicationDisplayName()),
                extras, QDialogButtonBox::Cancel, QDialogButtonBox::NoButton, parentWidget);

            dialog.addButton(tr("Restart"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);

            if (dialog.exec() == QDialogButtonBox::Cancel)
                return Qn::IncompatibleVersionConnectionResult;
        }

        nx::utils::Url serverUrl = connectionInfo.ecUrl;
        if (serverUrl.scheme().isEmpty())
            serverUrl.setScheme(nx::network::http::urlSheme(connectionInfo.allowSslConnections));

        QString authString = QnStartupParameters::createAuthenticationString(serverUrl,
            connectionInfo.version);

        switch (restartClient(connectionInfo.version, authString))
        {
            case ResultType::ok:
                return Qn::IncompatibleProtocolConnectionResult;

            case ResultType::connectError:
                return handleApplauncherError();

            default:
            {
                // trying to restore installation
                const auto version = connectionInfo.version.toString(
                    nx::utils::SoftwareVersion::MinorFormat);
                QnMessageBox dialog(QnMessageBoxIcon::Critical,
                    tr("Failed to download and launch version %1").arg(version),
                    QString(),
                    QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
                    parentWidget);

                dialog.addButton(
                    tr("Try Again"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);

                if (dialog.exec() == QDialogButtonBox::Cancel)
                    return Qn::IncompatibleVersionConnectionResult;

                //starting installation
                QScopedPointer<Dialog> installationDialog(
                    new Dialog(connectionInfo, parentWidget, engineVersion));
                installationDialog->exec();
                auto result = installationDialog->installationResult();
                if (result == Dialog::InstallResult::complete)
                {
                    isInstalled = true;
                    shouldAutoRestart = installationDialog->shouldAutoRestart();
                    continue;   //offering to start newly-installed compatibility version
                }
                else
                {
                    QnMessageBox dialog(QnMessageBoxIcon::Critical,
                        tr("Failed to enter compatibility mode for version %1").arg(versionString), extras,
                        QDialogButtonBox::Ok, QDialogButtonBox::NoButton, parentWidget);
                    dialog.exec();
                    return Qn::IncompatibleVersionConnectionResult;
                }
            }
        } // switch restartClient

    } // while(true)

    /* Just in case, should never get here. */
    NX_ASSERT(false, "Should never get here");
    return Qn::IncompatibleVersionConnectionResult;
}

QString QnConnectionDiagnosticsHelper::getErrorString(ErrorStrings id)
{
    switch (id)
    {
        case ErrorStrings::ContactAdministrator:
            return tr("If this error persists, please contact your VMS administrator.");
        case ErrorStrings::UnableConnect:
            return tr("Unable to connect to the server");
        case ErrorStrings::CloudIsNotReady:
            return tr("Connection to %1 is not ready yet. "
                "Check server Internet connection or try again later.",
                "%1 is the cloud name (like Nx Cloud)").arg(nx::network::AppInfo::cloudName());
        default:
            NX_ASSERT(false, "Should never get here");
            break;
    }
    return QString();
}

void QnConnectionDiagnosticsHelper::failedRestartClientMessage(QWidget* parent)
{
    QnMessageBox::critical(parent,
        tr("Failed to restart %1").arg(QnClientAppInfo::applicationDisplayName()),
        tr("Please close the application and start it again using the shortcut in the start menu."));
}
