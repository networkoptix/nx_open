#include "connection_diagnostics_helper.h"

#include <api/model/connection_info.h>

#include <client/client_settings.h>

#include <nx_ec/ec_api.h>

#include <ui/dialogs/message_box.h>
#include <ui/dialogs/compatibility_version_installation_dialog.h>
#include <ui/help/help_topics.h>

#include <utils/applauncher_utils.h>
#include <utils/common/software_version.h>

#include "compatibility.h"
#include "version.h"

namespace {
    QnSoftwareVersion minSupportedVersion("1.4"); 
}

QnConnectionDiagnosticsHelper::Result QnConnectionDiagnosticsHelper::validateConnection(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode, const QUrl &url, QWidget* parentWidget) {
    bool success = (errorCode == ec2::ErrorCode::ok);

    //checking brand compatibility
    if (success)
        success |= qnSettings->isDevMode() || connectionInfo.brand.isEmpty() || connectionInfo.brand == QLatin1String(QN_PRODUCT_NAME_SHORT);

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

    if (compatibilityChecker->isCompatible(QLatin1String("Client"), QnSoftwareVersion(QN_ENGINE_VERSION), QLatin1String("ECS"), connectionInfo.version)) 
        return Result::Success;

    if (connectionInfo.version < minSupportedVersion) {
        QnMessageBox::warning(
            parentWidget,
            Qn::VersionMismatch_Help,
            tr("Could not connect to Server"),
            tr("You are about to connect to Server which has a different version:\n"
            " - Client version: %1.\n"
            " - Server version: %2.\n"
            "Compatibility mode for versions lower than %3 is not supported."
            ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()).arg(minSupportedVersion.toString()),
            QMessageBox::Ok
            );
        return Result::Failure;
    }

    if (connectionInfo.version > QnSoftwareVersion(QN_ENGINE_VERSION)) {
#ifndef Q_OS_MACX
        QnMessageBox::warning(
            parentWidget,
            Qn::VersionMismatch_Help,
            tr("Could not connect to Server"),
            tr("Selected Server has a different version:\n"
            " - Client version: %1.\n"
            " - Server version: %2.\n"
            "An error has occurred while trying to restart in compatibility mode."
            ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()),
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
            ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()),
            QMessageBox::Ok
            );
#endif
        return Result::Failure;
    }


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
                ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()),
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
                ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()),
                QMessageBox::Ok
                );
#endif
            return Result::Failure;
        }

        if (!isInstalled) {
            int selectedButton = QnMessageBox::warning(
                parentWidget,
                Qn::VersionMismatch_Help,
                tr("Could not connect to Server"),
                tr("You are about to connect to Server which has a different version:\n"
                " - Client version: %1.\n"
                " - Server version: %2.\n"
                "Client version %3 is required to connect to this Server.\n"
                "Download version %3?"
                ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()).arg(connectionInfo.version.toString(QnSoftwareVersion::MinorFormat)),
                QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel),
                QMessageBox::Cancel
                );
            if( selectedButton == QMessageBox::Ok ) {
                QScopedPointer<CompatibilityVersionInstallationDialog> installationDialog(new CompatibilityVersionInstallationDialog(parentWidget));
                //starting installation
                installationDialog->setVersionToInstall( connectionInfo.version );
                installationDialog->exec();
                if( installationDialog->installationSucceeded() )
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
            ).arg(QLatin1String(QN_ENGINE_VERSION)).arg(connectionInfo.version.toString()),
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
                    QScopedPointer<CompatibilityVersionInstallationDialog> installationDialog(new CompatibilityVersionInstallationDialog(parentWidget));
                    installationDialog->setVersionToInstall( connectionInfo.version );
                    installationDialog->exec();
                    if( installationDialog->installationSucceeded() )
                        continue;   //offering to start newly-installed compatibility version
                }
                return Result::Failure;
            }
        } // switch restartClient

    } // while(true)

    Q_ASSERT(false);    //should never get here
    return Result::Failure; //just in case
}
