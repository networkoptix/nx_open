#include "connection_diagnostics_helper.h"

#include <api/model/connection_info.h>

#include <common/common_module.h>
#include <client/client_settings.h>

#include <nx_ec/ec_api.h>
#include <nx_ec/ec_proto_version.h>

#include <ui/dialogs/message_box.h>
#include <ui/dialogs/compatibility_version_installation_dialog.h>
#include <ui/help/help_topics.h>

#include <utils/applauncher_utils.h>
#include <utils/common/software_version.h>

#include "compatibility.h"
#include <utils/common/app_info.h>

namespace {
    QnSoftwareVersion minSupportedVersion("1.4"); 
}

QnConnectionDiagnosticsHelper::Result QnConnectionDiagnosticsHelper::validateConnectionLight(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode) {
    bool success = (errorCode == ec2::ErrorCode::ok);

    //checking brand compatibility
    if (success)
        success = qnSettings->isDevMode() || connectionInfo.brand.isEmpty() || connectionInfo.brand == QnAppInfo::productNameShort();

    if(!success)
        return Result::Failure;

    if (connectionInfo.nxClusterProtoVersion != nx_ec::EC2_PROTO_VERSION)
        return Result::Failure;

    QnCompatibilityChecker remoteChecker(connectionInfo.compatibilityItems);
    QnCompatibilityChecker localChecker(localCompatibilityItems());

    QnCompatibilityChecker *compatibilityChecker;
    if (remoteChecker.size() > localChecker.size()) {
        compatibilityChecker = &remoteChecker;
    } else {
        compatibilityChecker = &localChecker;
    }

    return (compatibilityChecker->isCompatible(lit("Client"), QnSoftwareVersion(qnCommon->engineVersion().toString()), lit("ECS"), connectionInfo.version))
        ? Result::Success
        : Result::Failure;
}


QnConnectionDiagnosticsHelper::Result QnConnectionDiagnosticsHelper::validateConnection(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode, const QUrl &url, QWidget* parentWidget) {
    if (validateConnectionLight(connectionInfo, errorCode) == Result::Success)
        return Result::Success;

    //TODO: #GDM duplicated code, Result enum should be improved to handle all existing connect options
    bool success = (errorCode == ec2::ErrorCode::ok);

    //checking brand compatibility
    if (success)
        success = qnSettings->isDevMode() || connectionInfo.brand.isEmpty() || connectionInfo.brand == QnAppInfo::productNameShort();

    QString detail;

    if (errorCode == ec2::ErrorCode::unauthorized) {
        detail = tr("Login or password you have entered are incorrect, please try again.");
    } else if (errorCode != ec2::ErrorCode::ok) {
        detail = tr("Connection to the Server could not be established.") + L'\n' 
               + tr("Connection details that you have entered are incorrect, please try again.") + L'\n' 
               + tr("If this error persists, please contact your VMS administrator.");
    } else { //brand incompatible
        detail = tr("You are trying to connect to incompatible Server.");
    }

    if(!success) {
        QnMessageBox::warning(
            parentWidget,
            Qn::Login_Help,
            tr("Could not connect to Server"),
            detail
            );
        return Result::Failure;
    }

    QString versionDetails =                             
          tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString()) + L'\n' 
        + tr(" - Server version: %1.").arg(connectionInfo.version.toString()) + L'\n';

    QnCompatibilityChecker remoteChecker(connectionInfo.compatibilityItems);
    QnCompatibilityChecker localChecker(localCompatibilityItems());

    QnCompatibilityChecker *compatibilityChecker;
    if (remoteChecker.size() > localChecker.size()) {
        compatibilityChecker = &remoteChecker;
    } else {
        compatibilityChecker = &localChecker;
    }

    bool haveExactVersion = false;
    bool needExactVersion = false;

    if (compatibilityChecker->isCompatible(QLatin1String("Client"), QnSoftwareVersion(qnCommon->engineVersion().toString()), QLatin1String("ECS"), connectionInfo.version)) {
        if (connectionInfo.nxClusterProtoVersion == nx_ec::EC2_PROTO_VERSION)
            return Result::Success;

        needExactVersion = true;
        QList<QnSoftwareVersion> versions;
        if (applauncher::getInstalledVersions(&versions) == applauncher::api::ResultType::ok) {
            haveExactVersion = versions.contains(connectionInfo.version);
        } else {
            QString olderComponent = connectionInfo.nxClusterProtoVersion < nx_ec::EC2_PROTO_VERSION
                ? tr("Server")
                : tr("Client");
            QString message = tr("You are about to connect to Server which has a different version:") + L'\n' 
                            + versionDetails
                            + tr("These versions are not compatible. Please update your %1.").arg(olderComponent);
#ifdef _DEBUG
            message += lit("\nClient Proto: %1\nServer Proto: %2").arg(nx_ec::EC2_PROTO_VERSION).arg(connectionInfo.nxClusterProtoVersion);
#endif
            QnMessageBox::warning(
                parentWidget,
                Qn::VersionMismatch_Help,
                tr("Could not connect to Server"),
                message,
                QMessageBox::Ok
                );
            return Result::Failure;
        }
    }

    if (connectionInfo.version < minSupportedVersion) {
        QnMessageBox::warning(
            parentWidget,
            Qn::VersionMismatch_Help,
            tr("Could not connect to Server"),
            tr("You are about to connect to Server which has a different version:") + L'\n'
            + versionDetails
            + tr("Compatibility mode for versions lower than %1 is not supported.").arg(minSupportedVersion.toString()),
            QMessageBox::Ok
            );
        return Result::Failure;
    }

#ifdef Q_OS_MACX
    if (connectionInfo.version > QnSoftwareVersion(qnCommon->engineVersion().toString())) {
        QnMessageBox::warning(
            parentWidget,
            Qn::VersionMismatch_Help,
            tr("Could not connect to Server"),
            tr("Selected Server has a different version:") + L'\n'
            + versionDetails
            + tr("The other version of the Client is needed in order to establish the connection to this Server."),
            QMessageBox::Ok
            );
        return Result::Failure;
    }
#endif

    while (true) {
        bool isInstalled = false;
        if (applauncher::isVersionInstalled(connectionInfo.version, &isInstalled) != applauncher::api::ResultType::ok)
        {
#ifndef Q_OS_MACX
            QnMessageBox::warning(
                parentWidget,
                Qn::VersionMismatch_Help,
                tr("Could not connect to Server"),
                tr("Selected Server has a different version:") + L'\n'
                + versionDetails
                + tr("An error has occurred while trying to restart in compatibility mode."),
                QMessageBox::Ok
                );
#else
            QnMessageBox::warning(
                parentWidget,
                Qn::VersionMismatch_Help,
                tr("Could not connect to Server"),
                tr("Selected Server has a different version:") + L'\n'
                + versionDetails
                + tr("The other version of the Client is needed in order to establish the connection to this Server."),
                QMessageBox::Ok
                );
#endif
            return Result::Failure;
        }

        if (!isInstalled || (needExactVersion && !haveExactVersion)) {
            QString versionString = connectionInfo.version.toString(
                    CompatibilityVersionInstallationDialog::useUpdate(connectionInfo.version)
                        ? QnSoftwareVersion::FullFormat
                        : QnSoftwareVersion::MinorFormat);

            int selectedButton = QnMessageBox::warning(
                parentWidget,
                Qn::VersionMismatch_Help,
                tr("Could not connect to Server"),
                tr("You are about to connect to Server which has a different version:") + L'\n'
                + tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString()) + L'\n' 
                + tr(" - Server version: %1.").arg(versionString) + L'\n'
                + tr("Client version %1 is required to connect to this Server.").arg(versionString) + L'\n' 
                + tr("Download version %1?").arg(versionString),
                
                QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::Cancel),
                QMessageBox::Cancel
                );
            if( selectedButton == QMessageBox::Yes ) {
                QScopedPointer<CompatibilityVersionInstallationDialog> installationDialog(
                            new CompatibilityVersionInstallationDialog(connectionInfo.version, parentWidget));
                //starting installation
                installationDialog->exec();
                if (installationDialog->installationSucceeded()) {
                    haveExactVersion = true;
                    continue;   //offering to start newly-installed compatibility version
                }
            }
            return Result::Failure;
        }

        //version is installed, trying to run
        int button = QnMessageBox::warning(
            parentWidget,
            Qn::VersionMismatch_Help,
            tr("Could not connect to Server"),
            tr("You are about to connect to Server which has a different version:") + L'\n'
            + versionDetails
            + tr("Would you like to restart the Client in compatibility mode?"),
            QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel), 
            QMessageBox::Cancel
            );

        if (button != QMessageBox::Ok)
            return Result::Failure;

        switch( applauncher::restartClient(connectionInfo.version, url.toEncoded()) ) {
        case applauncher::api::ResultType::ok:
            return Result::Restart;

        case applauncher::api::ResultType::connectError:
            QMessageBox::critical(
                parentWidget,
                tr("Launcher process is not found"),
                tr("Cannot restart the Client in compatibility mode.") + L'\n' 
              + tr("Please close the application and start it again using the shortcut in the start menu.")
                );
            return Result::Failure;

        default:
            {
                //trying to restore installation
                int selectedButton = QnMessageBox::warning(
                    parentWidget,
                    Qn::VersionMismatch_Help,
                    tr("Failure"),
                    tr("Failed to launch compatibility version %1").arg(connectionInfo.version.toString(QnSoftwareVersion::MinorFormat)) + L'\n' 
                  + tr("Try to restore version %1?").arg(connectionInfo.version.toString(QnSoftwareVersion::MinorFormat)),
                    QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel),
                    QMessageBox::Cancel
                    );
                if( selectedButton == QMessageBox::Ok ) {
                    //starting installation
                    QScopedPointer<CompatibilityVersionInstallationDialog> installationDialog(new CompatibilityVersionInstallationDialog(connectionInfo.version, parentWidget));
                    installationDialog->exec();
                    if (installationDialog->installationSucceeded())
                        continue;   //offering to start newly-installed compatibility version
                }
                return Result::Failure;
            }
        } // switch restartClient

    } // while(true)

    Q_ASSERT(false);    //should never get here
    return Result::Failure; //just in case
}

QnConnectionDiagnosticsHelper::TestConnectionResult QnConnectionDiagnosticsHelper::validateConnectionTest(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode)
{
    QnConnectionDiagnosticsHelper::TestConnectionResult result;

    QnCompatibilityChecker remoteChecker(connectionInfo.compatibilityItems);
    QnCompatibilityChecker localChecker(localCompatibilityItems());

    QnCompatibilityChecker* compatibilityChecker;
    if (remoteChecker.size() > localChecker.size())
        compatibilityChecker = &remoteChecker;
    else
        compatibilityChecker = &localChecker;

    //TODO #GDM almost same code exists in QnConnectionDiagnosticsHelper::validateConnection

    result.result = Result::Success;
    result.helpTopicId = Qn::Empty_Help;

    QString versionDetails =                             
        tr(" - Client version: %1.").arg(qnCommon->engineVersion().toString()) + L'\n' 
      + tr(" - Server version: %1.").arg(connectionInfo.version.toString()) + L'\n';

    bool compatibleProduct = qnSettings->isDevMode() || connectionInfo.brand.isEmpty()
        || connectionInfo.brand == QnAppInfo::productNameShort();

    if (errorCode == ec2::ErrorCode::unauthorized) {
        result.result = Result::Failure;
        result.details = tr("Login or password you have entered are incorrect, please try again.");
        result.helpTopicId = Qn::Login_Help;
    } else if (errorCode != ec2::ErrorCode::ok) {
        result.result = Result::Failure;
        result.details = tr("Connection to the Server could not be established.") + L'\n' 
            + tr("Connection details that you have entered are incorrect, please try again.") + L'\n' 
            + tr("If this error persists, please contact your VMS administrator.");
        result.helpTopicId = Qn::Login_Help;
    } else if (!compatibleProduct) {
        result.result = Result::Failure;
        result.details = tr("You are trying to connect to incompatible Server.");
        result.helpTopicId = Qn::Login_Help;
    } else if (!compatibilityChecker->isCompatible(QLatin1String("Client"), qnCommon->engineVersion(), QLatin1String("ECS"), connectionInfo.version)) {
        result.helpTopicId = Qn::VersionMismatch_Help;
        QnSoftwareVersion minSupportedVersion("1.4");

        if (connectionInfo.version < minSupportedVersion) {
            result.details = tr("Server has a different version:") + L'\n' 
                + versionDetails
                + tr("Compatibility mode for versions lower than %1 is not supported.").arg(minSupportedVersion.toString());
            result.result = Result::Failure;

        } else {
            result.details = tr("Server has a different version:") + L'\n' 
                + versionDetails
                + tr("You will be asked to restart the client in compatibility mode.");
        }
    } else if (connectionInfo.nxClusterProtoVersion != nx_ec::EC2_PROTO_VERSION) {
        QString olderComponent = connectionInfo.nxClusterProtoVersion < nx_ec::EC2_PROTO_VERSION
            ? tr("Server")
            : tr("Client");
        result.details = tr("Server has a different version:") + L'\n' 
            + versionDetails
            + tr("You will be asked to update your %1").arg(olderComponent);
    }


    return result;
}
