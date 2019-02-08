#include "utils.h"

#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtWidgets/QApplication>

#include <common/common_module.h>

#include <client_core/client_core_module.h>

#include <nx/vms/utils/platform/autorun.h>
#include <client/client_installations_manager.h>

namespace nx::vms::client::desktop {

namespace {

// TODO: #GDM think about code duplication
QString toWindowsRegistryFormat(const QString& path)
{
    return L'"' + QDir::toNativeSeparators(path).toLower() + L'"';
}

QString binaryPath()
{
    const QFileInfo appLauncherFile = QnClientInstallationsManager::appLauncher();
    if (appLauncherFile.exists())
        return toWindowsRegistryFormat(appLauncherFile.canonicalFilePath());

    const QFileInfo miniLauncherFile = QnClientInstallationsManager::miniLauncher();
    if (miniLauncherFile.exists())
        return toWindowsRegistryFormat(miniLauncherFile.canonicalFilePath());

    return QString();
}

} // namespace

void setVideoWallAutorunEnabled(const QnUuid& videoWallUuid, bool value)
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const QString key = qApp->applicationName() + L' ' + videoWallUuid.toString();

    const QString path = binaryPath();
    if (path.isEmpty())
        value = false; // intentionally disable autorun if all goes bad

    QStringList arguments;
    arguments << lit("--videowall");
    arguments << videoWallUuid.toString();
    auto url = commonModule->currentUrl();
    url.setUserName(QString());
    url.setPassword(QString());
    arguments << lit("--auth");
    arguments << QString::fromUtf8(url.toEncoded());

    nx::vms::utils::setAutoRunEnabled(key, path + L' ' + arguments.join(L' '), value);
}

} // namespace nx::vms::client::desktop
