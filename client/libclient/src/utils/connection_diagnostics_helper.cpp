#include "connection_diagnostics_helper.h"

#include <api/model/connection_info.h>

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/self_updater.h>

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
        case Qn::SuccessConnectionResult:
            return Qn::Empty_Help;
        case Qn::NetworkErrorConnectionResult:
        case Qn::UnauthorizedConnectionResult:
        case Qn::LdapTemporaryUnauthorizedConnectionResult:
        case Qn::CloudTemporaryUnauthorizedConnectionResult:
        case Qn::IncompatibleInternalConnectionResult:
        case Qn::IncompatibleCloudHostConnectionResult:
        case Qn::ForbiddenConnectionResult:
            return Qn::Login_Help;
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
    const QnConnectionInfo& connectionInfo)
{
    QString versionDetails =
        tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString()) + L'\n'
        + tr(" - Server version: %1.").arg(connectionInfo.version.toString()) + L'\n';

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
        return tr("Connection to the %1 is not ready yet. Check media server internet connection or try again later.").
            arg(QnAppInfo::cloudName()) + L'\n' + getErrorString(ErrorStrings::ContactAdministrator);
    case Qn::ForbiddenConnectionResult:
        return tr("Operation is not permitted now. It could happen due to media server is restarting now. Please try again later.")
            + L'\n' + getErrorString(ErrorStrings::ContactAdministrator);
    case Qn::NetworkErrorConnectionResult:
        return tr("Connection to the Server could not be established.") + L'\n'
            + tr("Connection details that you have entered are incorrect, please try again.") + L'\n'
            + getErrorString(ErrorStrings::ContactAdministrator);
    case Qn::IncompatibleInternalConnectionResult:
    case Qn::IncompatibleCloudHostConnectionResult:
        return tr("You are trying to connect to incompatible Server.");
    case Qn::IncompatibleVersionConnectionResult:
    {
        return tr("Server has a different version:") + L'\n'
            + versionDetails
            + tr("Compatibility mode for versions lower than %1 is not supported.")
            .arg(QnConnectionValidator::minSupportedVersion().toString());
    }
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
    QWidget* parentWidget)
{
    const auto result = QnConnectionValidator::validateConnection(connectionInfo, errorCode);
    if (result == Qn::SuccessConnectionResult)
        return result;

    const auto helpTopicId = helpTopic(result);
    const QString description = getErrorDescription(result, connectionInfo);

    if (result == Qn::IncompatibleProtocolConnectionResult
        || result == Qn::IncompatibleCloudHostConnectionResult)
    {
        return handleCompatibilityMode(connectionInfo, parentWidget);
    }

    QnMessageBox::warning(
        parentWidget,
        helpTopicId,
        getErrorString(ErrorStrings::UnableConnect),
        description);

    return result;
}

QnConnectionDiagnosticsHelper::TestConnectionResult
QnConnectionDiagnosticsHelper::validateConnectionTest(
    const QnConnectionInfo& connectionInfo,
    ec2::ErrorCode errorCode)
{
    using namespace Qn;
    TestConnectionResult result;

    //TODO #GDM almost same code exists in QnConnectionDiagnosticsHelper::validateConnection

    result.result = QnConnectionValidator::validateConnection(connectionInfo, errorCode);
    result.helpTopicId = helpTopic(result.result);

    result.details = getErrorDescription(result.result, connectionInfo);
    return result;
}

bool QnConnectionDiagnosticsHelper::getInstalledVersions(
    QList<QnSoftwareVersion>* versions)
{
    /* Try to run applauncher if it is not running. */
    if (!applauncher::checkOnline())
        return false;

    const auto result = applauncher::getInstalledVersions(versions);
    if (result == applauncher::api::ResultType::ok)
        return true;

    static const int kMaxTries = 5;
    for (int i = 0; i < kMaxTries; ++i)
    {
        QThread::msleep(100);
        qApp->processEvents();
        if (applauncher::getInstalledVersions(versions) == applauncher::api::ResultType::ok)
            return true;
    }
    return false;
}

Qn::ConnectionResult QnConnectionDiagnosticsHelper::handleApplauncherError(QWidget* parentWidget)
{
    showApplauncherError(parentWidget);

    return Qn::IncompatibleVersionConnectionResult;
}

void QnConnectionDiagnosticsHelper::showApplauncherError(QWidget* parentWidget)
{
    QnMessageBox::_critical(parentWidget,
        tr("Failed to restart %1 Client in compatibility mode")
            .arg(QnAppInfo::productNameLong()),
        tr("Please close %1 Client and start it again using the shortcut in the start menu.")
            .arg(QnAppInfo::productNameLong()));
}

QString QnConnectionDiagnosticsHelper::getDiffVersionsText()
{
    return tr("Client and Server have different versions")
}

QString QnConnectionDiagnosticsHelper::getDiffVersionsExtra(
    const QString& clientVersion,
    const QString& serverVersion)
{
    return tr("Client - %1", "%1 is version").arg(clientVersion)
        + L'\n' + tr("Server - %1", "%1 is version").arg(serverVersion);
}

QString QnConnectionDiagnosticsHelper::getDiffVersionsFullText(
    const QString& clientVersion,
    const QString& serverVersion)
{
    return getDiffVersionsText() + lit(".\n")
        + getDiffVersionsExtra(clientVersion, serverVersion);
}

Qn::ConnectionResult QnConnectionDiagnosticsHelper::handleCompatibilityMode(
    const QnConnectionInfo &connectionInfo,
    QWidget* parentWidget)
{
    using namespace Qn;
    int helpTopicId = helpTopic(Qn::IncompatibleProtocolConnectionResult);

    const auto versionDetails =
        tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString())
        + L'\n'
        + tr(" - Server version: %1.").arg(connectionInfo.version.toString())
        + L'\n';

    QList<QnSoftwareVersion> versions;
    if (!getInstalledVersions(&versions))
        return handleApplauncherError(parentWidget);
    bool isInstalled = versions.contains(connectionInfo.version);

    while (true)
    {
        if (!isInstalled)
        {
            QString versionString = connectionInfo.version.toString(
                CompatibilityVersionInstallationDialog::useUpdate(connectionInfo.version)
                ? QnSoftwareVersion::FullFormat
                : QnSoftwareVersion::MinorFormat);

            const auto extras =
                getDiffVersionsFullText(qnCommon->engineVersion().toString(), versionString)
                + L'\n'
                + tr("You have to download another version of"
                    " %1 Client to connect to this Server.").arg(QnAppInfo::productNameLong());

            QnMessageBox dialog(QnMessageBoxIcon::Question,
                tr("Download Client version %1?").arg(versionString), extras,
                QDialogButtonBox::Cancel, QDialogButtonBox::Yes, parentWidget);

            dialog.addCustomButton(QnMessageBoxCustomButton::Download);
            if (dialog.exec() == QDialogButtonBox::Yes)
            {
                QScopedPointer<CompatibilityVersionInstallationDialog> installationDialog(
                    new CompatibilityVersionInstallationDialog(connectionInfo.version, parentWidget));
                //starting installation
                installationDialog->exec();
                if (installationDialog->installationSucceeded())
                {
                    isInstalled = true;
                    continue;   //offering to start newly-installed compatibility version
                }
            }
            return Qn::IncompatibleVersionConnectionResult;
        }

        //version is installed, trying to run
        int button = QnMessageBox::warning(
            parentWidget,
            helpTopicId,
            getErrorString(ErrorStrings::UnableConnect),
            tr("You are about to connect to Server which has a different version:") + L'\n'
            + versionDetails
            + tr("Would you like to restart the Client in compatibility mode?"),
            QDialogButtonBox::StandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel),
            QDialogButtonBox::Cancel
        );

        if (button != QDialogButtonBox::Ok)
            return Qn::IncompatibleVersionConnectionResult;

        switch (applauncher::restartClient(connectionInfo.version, connectionInfo.ecUrl.toEncoded()))
        {
            case applauncher::api::ResultType::ok:
                return Qn::IncompatibleProtocolConnectionResult;

            case applauncher::api::ResultType::connectError:
                return handleCompatibilityMode(parentWidget);

            default:
            {
                //trying to restore installation
                // TODO: #ynikitenkov same version ??
                const auto version = connectionInfo.version.toString(QnSoftwareVersion::MinorFormat);
                QnMessageBox dialog(QnMessageBoxIcon::Critical,
                    tr("Failed to launch version %1").arg(version)
                        + L'\n' + tr("Try to restore version %1?").arg(version),
                    QString(),
                    QDialogButtonBox::Cancel, QDialogButtonBox::Yes,
                    parentWidget);

                dialog.addCustomButton(QnMessageBoxCustomButton::Restore);

                if (dialog.exec() == QDialogButtonBox::Yes)
                {
                    //starting installation
                    QScopedPointer<CompatibilityVersionInstallationDialog> installationDialog(new CompatibilityVersionInstallationDialog(connectionInfo.version, parentWidget));
                    installationDialog->exec();
                    if (installationDialog->installationSucceeded())
                        continue;   //offering to start newly-installed compatibility version
                }
                return Qn::IncompatibleVersionConnectionResult;
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
        default:
            NX_ASSERT(false, Q_FUNC_INFO, "Should never get here");
            break;
    }
    return QString();
}
