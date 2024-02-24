// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_videowall_shortcut_helper.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <client/client_installations_manager.h>
#include <core/resource/videowall_resource.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/client/desktop/app_icons.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/videowall/utils.h>
#include <nx/vms/utils/platform/autorun.h>
#include <platform/platform_abstraction.h>

namespace {

QString toWindowsRegistryFormat(const QString& path)
{
    return '"' + QDir::toNativeSeparators(path).toLower() + '"';
}

QString binaryPath()
{
    const QFileInfo appLauncherFile = QnClientInstallationsManager::appLauncher();
    if (appLauncherFile.exists())
        return appLauncherFile.canonicalFilePath();

    const QFileInfo miniLauncherFile = QnClientInstallationsManager::miniLauncher();
    if (miniLauncherFile.exists())
        return miniLauncherFile.canonicalFilePath();

    return QString();
}

QString shortcutPath()
{
    QString result = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (result.isEmpty())
        result = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    return result;
}

/** Resource file id for videowall shortcut. */
constexpr int kVideoWallIconId = IDI_ICON_VIDEOWALL;

} // namespace

namespace nx::vms::client::desktop {

bool VideoWallShortcutHelper::shortcutExists(const QnVideoWallResourcePtr& videowall) const
{
    QString destinationPath = shortcutPath();
    if (destinationPath.isEmpty())
        return false;

    return qnPlatform->shortcuts()->shortcutExists(destinationPath, videowall->getName());
}

bool VideoWallShortcutHelper::createShortcut(const QnVideoWallResourcePtr& videowall) const
{
    QString destinationPath = shortcutPath();
    if (destinationPath.isEmpty())
        return false;

    QString appPath = binaryPath();
    if (appPath.isEmpty())
        return false;

    if (!NX_ASSERT(connection(), "Client must be connected"))
        return false;

    QStringList arguments;
    arguments << "--videowall";
    arguments << videowall->getId().toString();

    const nx::utils::Url serverUrl = nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setEndpoint(connectionAddress())
        .toUrl();

    arguments << "--auth";
    arguments << QString::fromUtf8(serverUrl.toEncoded());

    return qnPlatform->shortcuts()->createShortcut(
        appPath,
        destinationPath,
        videowall->getName(),
        arguments,
        kVideoWallIconId);
}

bool VideoWallShortcutHelper::deleteShortcut(const QnVideoWallResourcePtr& videowall) const
{
    QString destinationPath = shortcutPath();
    if (destinationPath.isEmpty())
        return true;

    return qnPlatform->shortcuts()->deleteShortcut(destinationPath, videowall->getName());
}

void VideoWallShortcutHelper::setVideoWallAutorunEnabled(
    const nx::Uuid& videowallUuid,
    bool value,
    const nx::utils::Url& serverUrl)
{
    NX_ASSERT(!value || serverUrl.isValid(), "Url must be filled when enabling the autorun");

    const QString key = qApp->applicationName() + ' ' + videowallUuid.toString();

    const QString path = toWindowsRegistryFormat(binaryPath());
    if (path.isEmpty())
        value = false; // intentionally disable autorun if all goes bad

    QStringList arguments;
    arguments << "--videowall";
    arguments << videowallUuid.toString();
    arguments << "--auth";
    arguments << QString::fromUtf8(serverUrl.toEncoded());

    nx::vms::utils::setAutoRunEnabled(key, path + ' ' + arguments.join(' '), value);
}

bool VideoWallShortcutHelper::canStartVideoWall(const QnVideoWallResourcePtr& videowall)
{
    nx::Uuid pcUuid = appContext()->localSettings()->pcUuid();
    if (pcUuid.isNull())
    {
        NX_ERROR(NX_SCOPE_TAG, "Pc UUID is null, cannot start Video Wall on this pc");
        return false;
    }

    const auto videowallItems = videowall->items()->getItems();
    return std::any_of(videowallItems.cbegin(), videowallItems.cend(),
        [&pcUuid](const QnVideoWallItem& item)
        {
            return item.pcUuid == pcUuid && !item.runtimeStatus.online;
        });
}

} // namespace nx::vms::client::desktop
