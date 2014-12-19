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
        success |= qnSettings->isDevMode() || connectionInfo.brand.isEmpty() || connectionInfo.brand == QnAppInfo::productNameShort();

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

    //TODO: #GDM duplicated code
    bool success = (errorCode == ec2::ErrorCode::ok);

    //checking brand compatibility
    if (success)
        success |= qnSettings->isDevMode() || connectionInfo.brand.isEmpty() || connectionInfo.brand == QnAppInfo::productNameShort();

    QString detail;

    if (errorCode == ec2::ErrorCode::unauthorized) {
        detail = tr("Login or password you have entered are incorrect, please try again.");
    } else if (errorCode != ec2::ErrorCode::ok) {
        detail = tr("Connection to the Server could not be established.\n"
            "Connection details that you have entered are incorrect, please try again.\n\n"
            "If this error persists, please contact your VMS administrator.");
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

    QnCompatibilityChecker remoteChecker(connectionInfo.compatibilityItems);
    QnCompatibilityChecker localChecker(localCompatibilityItems());

    QnCompatibilityChecker *compatibilityChecker;
    if (remoteChecker.size() > localChecker.size()) {
        compatibilityChecker = &remoteChecker;
    } else {
        compatibilityChecker = &localChecker;
    }

    if (compatibilityChecker->isCompatible(QLatin1String("Client"), QnSoftwareVersion(qnCommon->engineVersion().toString()), QLatin1String("ECS"), connectionInfo.version)) {

        if (connectionInfo.nxClusterProtoVersion != nx_ec::EC2_PROTO_VERSION) {
            QString olderComponent = connectionInfo.nxClusterProtoVersion < nx_ec::EC2_PROTO_VERSION
                ? tr("Server")
                : tr("Client");
            QString message = tr("You are about to connect to Server which has a different version:\n"
                " - Client version: %1.\n"
                " - Server version: %2.\n"
                "These versions are not compatible. Please update your %3"
                ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()).arg(olderComponent);
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

        return Result::Success;
    }

    if (connectionInfo.version < minSupportedVersion) {
        QnMessageBox::warning(
            parentWidget,
            Qn::VersionMismatch_Help,
            tr("Could not connect to Server"),
            tr("You are about to connect to Server which has a different version:\n"
            " - Client version: %1.\n"
            " - Server version: %2.\n"
            "Compatibility mode for versions lower than %3 is not supported."
            ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()).arg(minSupportedVersion.toString()),
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
            tr("Selected Server has a different version:\n"
            " - Client version: %1.\n"
            " - Server version: %2.\n"
            "The other version of the Client is needed in order to establish the connection to this Server."
            ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()),
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
                tr("Selected Server has a different version:\n"
                " - Client version: %1.\n"
                " - Server version: %2.\n"
                "An error has occurred while trying to restart in compatibility mode."
                ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()),
                QMessageBox::Ok
                );
#else
            QnMessageBox::warning(
                parentWidget,
                Qn::VersionMismatch_Help,
                tr("Could not connect to Server"),
                tr("Selected Server has a different version:\n"
                " - Client version: %1.\n"
                " - Server version: %2.\n"
                "The other version of the Client is needed in order to establish the connection to this Server."
                ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()),
                QMessageBox::Ok
                );
#endif
            return Result::Failure;
        }

        if (!isInstalled) {
            //updating using compatibility functionality is forbidden
            if( connectionInfo.version > qnCommon->engineVersion() )
            {
                //forbidding installing newer version by compatibility
                QnMessageBox::warning(
                    parentWidget,
                    Qn::VersionMismatch_Help,
                    tr("Could not connect to Server"),
                    tr("Selected Server has a different version:\n"
                        " - Client version: %1.\n"
                        " - EC version: %2.\n"
                        "You need to download client %3 to connect"
                    ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()).arg(qnCommon->engineVersion().toString()),
                    QMessageBox::Ok
                );
                return Result::Failure;
            }

            int selectedButton = QnMessageBox::warning(
                parentWidget,
                Qn::VersionMismatch_Help,
                tr("Could not connect to Server"),
                tr("You are about to connect to Server which has a different version:\n"
                " - Client version: %1.\n"
                " - Server version: %2.\n"
                "Client version %3 is required to connect to this Server.\n"
                "Download version %3?"
                ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()).arg(connectionInfo.version.toString(QnSoftwareVersion::MinorFormat)),
                QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel),
                QMessageBox::Cancel
                );
            if( selectedButton == QMessageBox::Ok ) {
                QScopedPointer<QnCompatibilityVersionInstallationDialog> installationDialog(
                            new QnCompatibilityVersionInstallationDialog(connectionInfo.version, parentWidget));
                //starting installation
                installationDialog->exec();
                if (installationDialog->installationSucceeded())
                    continue;   //offering to start newly-installed compatibility version
            }
            return Result::Failure;
        }

        //version is installed, trying to run
        int button = QnMessageBox::warning(
            parentWidget,
            Qn::VersionMismatch_Help,
            tr("Could not connect to Server"),
            tr("You are about to connect to Server which has a different version:\n"
            " - Client version: %1.\n"
            " - Server version: %2.\n"
            "Would you like to restart the Client in compatibility mode?"
            ).arg(qnCommon->engineVersion().toString()).arg(connectionInfo.version.toString()),
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
                tr("Cannot restart the Client in compatibility mode.\n"
                "Please close the application and start it again using the shortcut in the start menu.")
                );
            return Result::Failure;

        default:
            {
                //trying to restore installation
                int selectedButton = QnMessageBox::warning(
                    parentWidget,
                    Qn::VersionMismatch_Help,
                    tr("Failure"),
                    tr("Failed to launch compatibility version %1\n"
                    "Try to restore version %1?").
                    arg(connectionInfo.version.toString(QnSoftwareVersion::MinorFormat)),
                    QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel),
                    QMessageBox::Cancel
                    );
                if( selectedButton == QMessageBox::Ok ) {
                    //starting installation
                    QScopedPointer<QnCompatibilityVersionInstallationDialog> installationDialog(new QnCompatibilityVersionInstallationDialog(connectionInfo.version, parentWidget));
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
