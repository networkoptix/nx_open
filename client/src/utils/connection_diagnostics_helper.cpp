#include "connection_diagnostics_helper.h"

#include <api/model/connection_info.h>

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <nx_ec/ec_api.h>

#include <ui/dialogs/compatibility_version_installation_dialog.h>
#include <ui/help/help_topics.h>

#include <utils/applauncher_utils.h>
#include <utils/common/software_version.h>
#include <utils/common/app_info.h>

namespace
{
    QnSoftwareVersion kMinSupportedVersion("1.4");
}


QnConnectionDiagnosticsHelper::Result QnConnectionDiagnosticsHelper::validateConnectionLight(const QString &brand, int protoVersion)
{
    if (protoVersion != QnAppInfo::ec2ProtoVersion())
        return Result::IncompatibleProtocol;

    //checking brand compatibility
    bool compatibleBrand = qnRuntime->isDevMode() || brand.isEmpty() || brand == QnAppInfo::productNameShort();
    return compatibleBrand
        ? Result::Success
        : Result::IncompatibleBrand;
}


QnConnectionDiagnosticsHelper::Result QnConnectionDiagnosticsHelper::validateConnectionLight(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode)
{
    if (errorCode == ec2::ErrorCode::unauthorized)
        return Result::Unauthorized;

    if (errorCode == ec2::ErrorCode::temporary_unauthorized)
        return Result::TemporaryUnauthorized;

    if (errorCode != ec2::ErrorCode::ok)
        return Result::ServerError;

    return validateConnectionLight(connectionInfo.brand, connectionInfo.nxClusterProtoVersion);
}


QnConnectionDiagnosticsHelper::Result QnConnectionDiagnosticsHelper::validateConnection(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode, const QUrl &url, QWidget* parentWidget)
{
    QnConnectionDiagnosticsHelper::Result result = validateConnectionLight(connectionInfo, errorCode);
    if (result == Result::Success)
        return result;

    QString detail;
    if (result == Result::Unauthorized) {
        detail = tr("The username or password you have entered is incorrect. Please try again.");
    } else if (result == Result::TemporaryUnauthorized) {
        detail = tr("LDAP Server connection timed out.") + L'\n'
            + strings(ErrorStrings::ContactAdministrator);
    } else if (result == Result::ServerError) {
        detail = tr("Connection to the Server could not be established.") + L'\n'
               + tr("Connection details that you have entered are incorrect, please try again.") + L'\n'
               + strings(ErrorStrings::ContactAdministrator);
    } else if (result == Result::IncompatibleBrand) { //brand incompatible
        detail = tr("You are trying to connect to incompatible Server.");
    }

    if(!detail.isEmpty()) {
        QnMessageBox::warning(
            parentWidget,
            Qn::Login_Help,
            strings(ErrorStrings::UnableConnect),
            detail
            );
        return result;
    }

    QString versionDetails =
          tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString()) + L'\n'
        + tr(" - Server version: %1.").arg(connectionInfo.version.toString()) + L'\n';

    bool haveExactVersion = false;
    bool needExactVersion = false;

    if (qnCommon->engineVersion().isCompatible(connectionInfo.version))
    {
        if (connectionInfo.nxClusterProtoVersion == QnAppInfo::ec2ProtoVersion())
            return Result::Success;

        needExactVersion = true;
        QList<QnSoftwareVersion> versions;
        if (applauncher::getInstalledVersions(&versions) == applauncher::api::ResultType::ok) {
            haveExactVersion = versions.contains(connectionInfo.version);
        } else {
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
                Qn::VersionMismatch_Help,
                strings(ErrorStrings::UnableConnect),
                message,
                QDialogButtonBox::Ok
                );
            return Result::IncompatibleVersion;
        }
    }

    if (connectionInfo.version < kMinSupportedVersion)
    {
        QnMessageBox::warning(
            parentWidget,
            Qn::VersionMismatch_Help,
            strings(ErrorStrings::UnableConnect),
            tr("You are about to connect to Server which has a different version:") + L'\n'
            + versionDetails
            + tr("Compatibility mode for versions lower than %1 is not supported.").arg(kMinSupportedVersion.toString()),
            QDialogButtonBox::Ok
            );
        return Result::IncompatibleVersion;
    }

    while (true) {
        bool isInstalled = false;
        if (applauncher::isVersionInstalled(connectionInfo.version, &isInstalled) != applauncher::api::ResultType::ok)
        {
            QnMessageBox::warning(
                parentWidget,
                Qn::VersionMismatch_Help,
                strings(ErrorStrings::UnableConnect),
                tr("Selected Server has a different version:") + L'\n'
                + versionDetails
                + tr("An error has occurred while trying to restart in compatibility mode."),
                QDialogButtonBox::Ok
                );
            return Result::IncompatibleVersion;
        }

        if (!isInstalled || (needExactVersion && !haveExactVersion)) {
            QString versionString = connectionInfo.version.toString(
                    CompatibilityVersionInstallationDialog::useUpdate(connectionInfo.version)
                        ? QnSoftwareVersion::FullFormat
                        : QnSoftwareVersion::MinorFormat);

            int selectedButton = QnMessageBox::warning(
                parentWidget,
                Qn::VersionMismatch_Help,
                strings(ErrorStrings::UnableConnect),
                tr("You are about to connect to Server which has a different version:") + L'\n'
                + tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString()) + L'\n'
                + tr(" - Server version: %1.").arg(versionString) + L'\n'
                + tr("Client version %1 is required to connect to this Server.").arg(versionString) + L'\n'
                + tr("Download version %1?").arg(versionString),

                QDialogButtonBox::StandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::Cancel),
                QDialogButtonBox::Cancel
                );
            if (selectedButton == QDialogButtonBox::Yes) {
                QScopedPointer<CompatibilityVersionInstallationDialog> installationDialog(
                            new CompatibilityVersionInstallationDialog(connectionInfo.version, parentWidget));
                //starting installation
                installationDialog->exec();
                if (installationDialog->installationSucceeded()) {
                    haveExactVersion = true;
                    continue;   //offering to start newly-installed compatibility version
                }
            }
            return Result::IncompatibleVersion;
        }

        //version is installed, trying to run
        int button = QnMessageBox::warning(
            parentWidget,
            Qn::VersionMismatch_Help,
            strings(ErrorStrings::UnableConnect),
            tr("You are about to connect to Server which has a different version:") + L'\n'
            + versionDetails
            + tr("Would you like to restart the Client in compatibility mode?"),
            QDialogButtonBox::StandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel),
            QDialogButtonBox::Cancel
            );

        if (button != QDialogButtonBox::Ok)
            return Result::IncompatibleVersion;

        switch( applauncher::restartClient(connectionInfo.version, url.toEncoded()) ) {
        case applauncher::api::ResultType::ok:
            return Result::RestartRequested;

        case applauncher::api::ResultType::connectError:
            QnMessageBox::critical(
                parentWidget,
                tr("Launcher process not found."),
                tr("Cannot restart the Client in compatibility mode.") + L'\n'
              + tr("Please close the application and start it again using the shortcut in the start menu.")
                );
            return Result::IncompatibleVersion;

        default:
            {
                //trying to restore installation
                int selectedButton = QnMessageBox::warning(
                    parentWidget,
                    Qn::VersionMismatch_Help,
                    tr("Failure"),
                    tr("Failed to launch compatibility version %1").arg(connectionInfo.version.toString(QnSoftwareVersion::MinorFormat)) + L'\n'
                  + tr("Try to restore version %1?").arg(connectionInfo.version.toString(QnSoftwareVersion::MinorFormat)),
                    QDialogButtonBox::StandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel),
                    QDialogButtonBox::Cancel
                    );
                if( selectedButton == QDialogButtonBox::Ok ) {
                    //starting installation
                    QScopedPointer<CompatibilityVersionInstallationDialog> installationDialog(new CompatibilityVersionInstallationDialog(connectionInfo.version, parentWidget));
                    installationDialog->exec();
                    if (installationDialog->installationSucceeded())
                        continue;   //offering to start newly-installed compatibility version
                }
                return Result::IncompatibleVersion;
            }
        } // switch restartClient

    } // while(true)

    NX_ASSERT(false);    //should never get here
    return Result::IncompatibleVersion; //just in case
}

QnConnectionDiagnosticsHelper::TestConnectionResult QnConnectionDiagnosticsHelper::validateConnectionTest(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode)
{
    QnConnectionDiagnosticsHelper::TestConnectionResult result;

    //TODO #GDM almost same code exists in QnConnectionDiagnosticsHelper::validateConnection

    result.result = Result::Success;
    result.helpTopicId = Qn::Empty_Help;

    QString versionDetails =
        tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString()) + L'\n'
      + tr(" - Server version: %1.").arg(connectionInfo.version.toString()) + L'\n';

    bool compatibleProduct = qnRuntime->isDevMode() || connectionInfo.brand.isEmpty()
        || connectionInfo.brand == QnAppInfo::productNameShort();

    if (errorCode == ec2::ErrorCode::unauthorized)
    {
        result.result = Result::Unauthorized;
        result.details = tr("The username or password you have entered is incorrect. Please try again.");
        result.helpTopicId = Qn::Login_Help;
    }
    else if (errorCode == ec2::ErrorCode::temporary_unauthorized)
    {
        result.result = Result::TemporaryUnauthorized;
        result.details = tr("LDAP Server connection timed out.") + L'\n'
            + strings(ErrorStrings::ContactAdministrator);
        result.helpTopicId = Qn::Login_Help;
    }
    else if (errorCode != ec2::ErrorCode::ok)
    {
        result.result = Result::ServerError;
        result.details = tr("Connection to the Server could not be established.") + L'\n'
            + tr("Connection details that you have entered are incorrect, please try again.") + L'\n'
            + strings(ErrorStrings::ContactAdministrator);
        result.helpTopicId = Qn::Login_Help;
    }
    else if (!compatibleProduct)
    {
        result.result = Result::IncompatibleBrand;
        result.details = tr("You are trying to connect to incompatible Server.");
        result.helpTopicId = Qn::Login_Help;
    }
    else if (!qnCommon->engineVersion().isCompatible(connectionInfo.version))
    {
        result.helpTopicId = Qn::VersionMismatch_Help;

        if (connectionInfo.version < kMinSupportedVersion)
        {
            result.details = tr("Server has a different version:") + L'\n'
                + versionDetails
                + tr("Compatibility mode for versions lower than %1 is not supported.").arg(kMinSupportedVersion.toString());
            result.result = Result::IncompatibleVersion;

        }
        else
        {
            result.details = tr("Server has a different version:") + L'\n'
                + versionDetails
                + tr("You will be asked to restart the client in compatibility mode.");
        }
    }
    else if (connectionInfo.nxClusterProtoVersion != QnAppInfo::ec2ProtoVersion())
    {
        result.helpTopicId = Qn::VersionMismatch_Help;
        QString olderComponent = connectionInfo.nxClusterProtoVersion < QnAppInfo::ec2ProtoVersion()
            ? tr("Server")
            : tr("Client");
        result.details = tr("Server has a different version:") + L'\n'
            + versionDetails
            + tr("You will be asked to update your %1").arg(olderComponent);
    }

    return result;
}

#ifdef _DEBUG
QString QnConnectionDiagnosticsHelper::resultToString(Result value) {
    switch (value)
    {
    case QnConnectionDiagnosticsHelper::Result::Success:
        return lit("Success");
    case QnConnectionDiagnosticsHelper::Result::RestartRequested:
        return lit("RestartRequested");
    case QnConnectionDiagnosticsHelper::Result::IncompatibleBrand:
        return lit("IncompatibleBrand");
    case QnConnectionDiagnosticsHelper::Result::IncompatibleVersion:
        return lit("IncompatibleVersion");
    case QnConnectionDiagnosticsHelper::Result::IncompatibleProtocol:
        return lit("IncompatibleProtocol");
    case QnConnectionDiagnosticsHelper::Result::Unauthorized:
        return lit("Unauthorized");
    case QnConnectionDiagnosticsHelper::Result::ServerError:
        return lit("ServerError");
    default:
        NX_ASSERT(false, Q_FUNC_INFO, "Should never get here");
        break;
    }
    return QString();
}

#endif

QString QnConnectionDiagnosticsHelper::strings(ErrorStrings id) {
    switch (id) {
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

