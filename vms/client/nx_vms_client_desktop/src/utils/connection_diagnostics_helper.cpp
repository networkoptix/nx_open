// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_diagnostics_helper.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>
#include <QtWidgets/QPushButton>

#include <api/runtime_info_manager.h>
#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <client/self_updater.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <nx/branding.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/client/core/network/remote_connection_error_strings.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_update/compatibility_version_installation_dialog.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>
#include <utils/applauncher_utils.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;
using namespace nx::vms::common;

using RemoteConnectionErrorCode = nx::vms::client::core::RemoteConnectionErrorCode;

namespace {

static const auto kCloudTokenVersion = nx::utils::SoftwareVersion(5, 0);

QString getDiffVersionsText()
{
    return QnConnectionDiagnosticsHelper::tr("Client and Server have different versions");
}

QString getDiffVersionsExtra(
    const QString& clientVersion,
    const QString& serverVersion)
{
    return QnConnectionDiagnosticsHelper::tr("Client - %1", "%1 is version").arg(clientVersion)
        + html::kLineBreak
        + QnConnectionDiagnosticsHelper::tr("Server - %1", "%1 is version").arg(serverVersion);
}

QString getDiffVersionsFullText(
    const QString& clientVersion,
    const QString& serverVersion)
{
    return getDiffVersionsText() + ":"
        + html::kLineBreak
        + getDiffVersionsExtra(clientVersion, serverVersion);
}

QString getDiffVersionFullExtras(
    const nx::vms::api::ModuleInformation& moduleInformation,
    const QString& extraText,
    const nx::utils::SoftwareVersion& engineVersion)
{
    const QString clientVersion = engineVersion.toString();
    const QString serverVersion = moduleInformation.version.toString();

    QString result = getDiffVersionsFullText(clientVersion, serverVersion)
        + "\n" + extraText;
    if (ini().developerMode)
    {
        result += '\n' + QnConnectionDiagnosticsHelper::developerModeText(
            moduleInformation,
            engineVersion);
    }
    return result;
}

void showApplauncherErrorDialog(
    const nx::vms::api::ModuleInformation& moduleInformation,
    const nx::utils::SoftwareVersion& engineVersion,
    QWidget* parentWidget)
{
    const QString extras = getDiffVersionFullExtras(moduleInformation,
        QnConnectionDiagnosticsHelper::tr(
            "Please close %1 and start it again using the shortcut in the start menu.")
        .arg(nx::branding::desktopClientDisplayName()),
        engineVersion);

    QnMessageBox::critical(parentWidget,
        QnConnectionDiagnosticsHelper::tr("Failed to restart %1 in compatibility mode")
        .arg(nx::branding::desktopClientDisplayName()),
        extras);
}

bool agreeToDownloadVersion(
    const nx::vms::api::ModuleInformation& moduleInformation,
    const nx::utils::SoftwareVersion& engineVersion,
    QWidget* parentWidget)
{
    auto extras = getDiffVersionFullExtras(moduleInformation,
        QnConnectionDiagnosticsHelper::tr(
            "You have to download another version of %1 to connect to this Server.")
            .arg(nx::branding::desktopClientDisplayName()),
        engineVersion);

    QnMessageBox dialog(QnMessageBoxIcon::Question,
        QnConnectionDiagnosticsHelper::tr("Download Client version %1?")
            .arg(moduleInformation.version.toString()),
        extras,
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        parentWidget);

    dialog.addButton(QnConnectionDiagnosticsHelper::tr("Download && Install"),
        QDialogButtonBox::AcceptRole,
        Qn::ButtonAccent::Standard);

    return dialog.exec() != QDialogButtonBox::Cancel;
}

std::pair<bool /*success*/, bool /*confirmRestart*/> downloadCompatibleVersion(
    const nx::vms::api::ModuleInformation& moduleInformation,
    const nx::vms::client::core::LogonData& logonData,
    const nx::utils::SoftwareVersion& engineVersion,
    QWidget* parentWidget)
{
    QScopedPointer<CompatibilityVersionInstallationDialog> installationDialog(
        new CompatibilityVersionInstallationDialog(
            moduleInformation, logonData, engineVersion, parentWidget));

    // Starting installation
    if (installationDialog->exec() == QDialog::Rejected)
        return {/*success*/ false, /*confirmRestart*/ false};

    const auto installationResult = installationDialog->installationResult();
    if (installationResult == CompatibilityVersionInstallationDialog::InstallResult::complete)
    {
        return {/*success*/ true, /*confirmRestart*/ !installationDialog->shouldAutoRestart()};
    }

    const auto extras = getDiffVersionFullExtras(moduleInformation,
        QnConnectionDiagnosticsHelper::tr(
            "You have to download another version of %1 to connect to this Server.")
            .arg(nx::branding::desktopClientDisplayName()),
        engineVersion);

    QString errorString = installationDialog->errorString();
    if (!errorString.isEmpty())
        errorString = QString("%1\n%2").arg(errorString, extras);
    else
        errorString = extras;

    QnConnectionDiagnosticsHelper::showCompatibilityModeFailureMessage(
        moduleInformation.version, errorString, parentWidget);

    return {/*success*/ false, /*confirmRestart*/ false};
}

bool confirmRestart(
    const nx::vms::api::ModuleInformation& moduleInformation,
    const nx::utils::SoftwareVersion& engineVersion,
    QWidget* parentWidget)
{
    const auto extras = getDiffVersionFullExtras(moduleInformation,
        QnConnectionDiagnosticsHelper::tr(
            "You have to restart %1 in compatibility mode to connect to this Server."
        ).arg(nx::branding::desktopClientDisplayName()),
        engineVersion);

    QnMessageBox dialog(QnMessageBoxIcon::Question,
        QnConnectionDiagnosticsHelper::tr("Restart %1 in compatibility mode?")
            .arg(nx::branding::desktopClientDisplayName()),
        extras,
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        parentWidget);

    dialog.addButton(
        QnConnectionDiagnosticsHelper::tr("Restart"),
        QDialogButtonBox::AcceptRole,
        Qn::ButtonAccent::Standard);

    return dialog.exec() != QDialogButtonBox::Cancel;
}

bool agreeToTryAgain(
    const nx::vms::api::ModuleInformation& moduleInformation,
    QWidget* parentWidget)
{
    const QString version =
        moduleInformation.version.toString(nx::utils::SoftwareVersion::Format::minor);
    QnMessageBox dialog(QnMessageBoxIcon::Critical,
        QnConnectionDiagnosticsHelper::tr("Failed to download and launch version %1").arg(version),
        /*extras*/ QString(),
        QDialogButtonBox::Cancel,
        QDialogButtonBox::NoButton,
        parentWidget);

    dialog.addButton(
        QnConnectionDiagnosticsHelper::tr("Try Again"),
        QDialogButtonBox::AcceptRole,
        Qn::ButtonAccent::Standard);

    return dialog.exec() != QDialogButtonBox::Cancel;
}

} // namespace

bool QnConnectionDiagnosticsHelper::downloadAndRunCompatibleVersion(
    QWidget* parentWidget,
    const nx::vms::api::ModuleInformation& moduleInformation,
    nx::vms::client::core::LogonData logonData,
    const nx::utils::SoftwareVersion& engineVersion)
{
    using namespace nx::vms::applauncher::api;

    QList<nx::utils::SoftwareVersion> versions;
    if (!getInstalledVersions(&versions))
    {
        showApplauncherErrorDialog(moduleInformation, engineVersion, parentWidget);
        return false;
    }

    QList<QString> versionStrings;
    for (auto version: versions)
        versionStrings.append(version.toString());
    NX_VERBOSE(NX_SCOPE_TAG, "The following client versions are installed: %1", versionStrings);

    bool isInstalled = versions.contains(moduleInformation.version);
    bool needToConfirmRestart = true;

    if (!isInstalled && !agreeToDownloadVersion(moduleInformation, engineVersion, parentWidget))
        return false;

    const QString authString = QnStartupParameters::createAuthenticationString(
        logonData, moduleInformation.version);

    // Version is installed, trying to run.
    while (true)
    {
        if (!isInstalled)
        {
            auto [success, confirmRestart] = downloadCompatibleVersion(
                moduleInformation, logonData, engineVersion, parentWidget);
            if (!success)
                return false;

            needToConfirmRestart = confirmRestart;
        }

        if (needToConfirmRestart && !confirmRestart(moduleInformation, engineVersion, parentWidget))
            return false;

        switch (restartClient(moduleInformation.version, authString))
        {
            case ResultType::ok:
            {
                return true;
            }

            case ResultType::connectError:
            {
                showApplauncherErrorDialog(moduleInformation, engineVersion, parentWidget);
                return false;
            }

            default:
            {
                if (agreeToTryAgain(moduleInformation, parentWidget))
                    isInstalled = false; //< Re-download the compatible version.
                else
                    return false;
            }
        }
    }

    // Suppressing compiler warning, should never get here.
    NX_ASSERT(false, "Should never get here");
    return false;
}

void QnConnectionDiagnosticsHelper::showConnectionErrorMessage(
    QnWorkbenchContext* context,
    RemoteConnectionError error,
    const nx::vms::api::ModuleInformation& moduleInformation,
    const nx::utils::SoftwareVersion& engineVersion,
    QWidget* parentWidget)
{
    if (!parentWidget)
        parentWidget = context->mainWindowWidget();

    QnMessageBox::Icon icon;
    switch (error.code)
    {
        case RemoteConnectionErrorCode::sessionExpired:
        case RemoteConnectionErrorCode::temporaryTokenExpired:
        case RemoteConnectionErrorCode::cloudSessionExpired:
        case RemoteConnectionErrorCode::unauthorized:
        case RemoteConnectionErrorCode::userIsDisabled:
        case RemoteConnectionErrorCode::twoFactorAuthOfCloudUserIsDisabled:
            icon = QnMessageBox::Icon::Warning;
            break;
        default:
            icon = QnMessageBox::Icon::Critical;
            break;
    }

    const auto getShortErrorDescriptionText =
        [&error, &moduleInformation, &engineVersion]
        {
            return nx::vms::client::core::errorDescription(
                error.code, moduleInformation, engineVersion).shortText;
        };

    QString title;
    switch (error.code)
    {
        case RemoteConnectionErrorCode::sessionExpired:
        case RemoteConnectionErrorCode::temporaryTokenExpired:
        case RemoteConnectionErrorCode::cloudSessionExpired:
        case RemoteConnectionErrorCode::ldapInitializationInProgress:
            title = getShortErrorDescriptionText();
            break;
        case RemoteConnectionErrorCode::systemIsNotCompatibleWith2Fa:
            title = tr("System is not compatible with two-factor authentication");
            break;
        case RemoteConnectionErrorCode::twoFactorAuthOfCloudUserIsDisabled:
            title = tr("Failed to log in to System \"%1\"").arg(moduleInformation.systemName);
            break;

        default:
            title = tr("Failed to connect to Server");
            break;
    }

    QString extras = error.externalDescription
        ? *error.externalDescription
        : nx::vms::client::core::errorDescription(
            error.code,
            moduleInformation,
            engineVersion).longText;

    if (error.code == RemoteConnectionErrorCode::cloudSessionExpired)
    {
        if (!NX_ASSERT(QThread::currentThread() == qApp->thread(),
            "Trying to send report not in GUI thread."))
        {
            return;
        }

        static QnMessageBox* msgBox = nullptr;
        if (msgBox)
            return;
        msgBox = new QnMessageBox(icon,
            title,
            extras,
            /*buttons*/ QDialogButtonBox::Ok,
            /*defaultButton*/ QDialogButtonBox::NoButton,
            parentWidget);

        auto okButton = msgBox->button(QDialogButtonBox::Ok);
        okButton->setText(tr("Log In..."));
        connect(okButton,
            &QPushButton::clicked,
            [context]() { context->menu()->trigger(ui::action::LoginToCloud); });

        msgBox->exec();
        msgBox->deleteLater();
        msgBox = nullptr;
        return;
    }

    QnMessageBox msgBox(
        icon,
        title,
        extras,
        /*buttons*/ QDialogButtonBox::Ok,
        /*defaultButton*/ QDialogButtonBox::NoButton,
        parentWidget);
    msgBox.setInformativeTextFormat(Qt::TextFormat::RichText);

    connect(&msgBox, &QnMessageBox::linkActivated, &msgBox,
        [context](const QString& link)
        {
            if (link == "#cloud")
                context->menu()->trigger(ui::action::LoginToCloud);
            else if (link == "#cloud_account_security")
                context->menu()->trigger(ui::action::OpenCloudAccountSecurityUrl);
        });

    msgBox.exec();
}

void QnConnectionDiagnosticsHelper::showCompatibilityModeFailureMessage(
        const nx::utils::SoftwareVersion& version,
        const QString& errorDescription,
        QWidget* parentWidget)
{
    QnMessageBox dialog(QnMessageBoxIcon::Critical,
        QnConnectionDiagnosticsHelper::tr("Failed to enter compatibility mode for version %1")
            .arg(version.toString()),
        errorDescription,
        QDialogButtonBox::Ok,
        QDialogButtonBox::NoButton,
        parentWidget);
    dialog.exec();
}

void QnConnectionDiagnosticsHelper::showTemporaryUserReAuthMessage(QWidget* parentWidget)
{
    QnMessageBox dialog(QnMessageBoxIcon::Warning,
        QnConnectionDiagnosticsHelper::tr("Your session has expired"),
        QnConnectionDiagnosticsHelper::tr("Please sign in again with your link to continue"),
        QDialogButtonBox::Ok,
        QDialogButtonBox::NoButton,
        parentWidget);
    dialog.exec();
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

void QnConnectionDiagnosticsHelper::failedRestartClientMessage(QWidget* parent)
{
    QnMessageBox::critical(parent,
        tr("Failed to restart %1").arg(nx::branding::desktopClientDisplayName()),
        tr("Please close the application and start it again using the shortcut in the start menu."));
}

QString QnConnectionDiagnosticsHelper::developerModeText(
    const nx::vms::api::ModuleInformation& moduleInformation,
    const nx::utils::SoftwareVersion& engineVersion)
{
    auto asError =
        [](const auto& value)
        {
            return nx::format("<font color=#F00>%1</font>", value).toQString();
        };

    auto asErrorIfNotEqualTo =
        [asError](const auto& value, const auto& base)
        {
            return value == base
                ? nx::format("%1", value).toQString()
                : asError(value);
        };

    QString result =
        "<hr><div style=\"text-indent: 0; font-size: 12px; color: #CCC;\">Developer info:<br/>";

    const QString clientCloudHost = QString::fromStdString(
        nx::network::SocketGlobals::cloud().cloudHost());

    QStringList details;
    details << nx::format("Client version: <b>%1</b>", engineVersion);
    details << nx::format("Client brand: <b>%1</b>", nx::branding::brand());
    details << nx::format("Client customization: <b>%1</b>", nx::branding::customization());

    details << nx::format("Client protocol: <b>%1</b>", nx::vms::api::protocolVersion());
    details << nx::format("Client cloud host: <b>%1</b>", clientCloudHost);
    details << html::kHorizontalLine;
    details << nx::format("Server version: <b>%1</b>", moduleInformation.version);
    details << nx::format("Server brand: <b>%1</b>",
        asErrorIfNotEqualTo(moduleInformation.brand, nx::branding::brand()));
    details << nx::format("Server customization: <b>%1</b>",
        asErrorIfNotEqualTo(moduleInformation.customization, nx::branding::customization()));
    details << nx::format("Server protocol: <b>%1</b>",
        asErrorIfNotEqualTo(moduleInformation.protoVersion, nx::vms::api::protocolVersion()));
    details << nx::format("Server cloud host: <b>%1</b>",
        asErrorIfNotEqualTo(moduleInformation.cloudHost, clientCloudHost));

    result += details.join(html::kLineBreak);

    result += "</div>";
    qDebug() << result;

    return result;
}
