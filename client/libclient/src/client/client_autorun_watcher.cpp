#include "client_autorun_watcher.h"

#include <client/client_settings.h>
#include <client/client_installations_manager.h>

#include <nx/vms/utils/platform/autorun.h>

#include <utils/common/app_info.h>

namespace {

const int kSettingsKey = QnClientSettings::AUTO_START;

QString toWindowsRegistryFormat(const QString& path)
{
    return L'"' + QDir::toNativeSeparators(path).toLower() + L'"';
}

} // namespace

QnClientAutoRunWatcher::QnClientAutoRunWatcher(QObject* parent)
{
    /* Write out the setting first, then listen to changes. */
    qnSettings->setAutoStart(isAutoRun());
    connect(qnSettings->notifier(kSettingsKey), &QnPropertyNotifier::valueChanged, this,
        [this]()
        {
            setAutoRun(qnSettings->autoStart());
        });
}

bool QnClientAutoRunWatcher::isAutoRun() const
{
    return nx::vms::utils::isAutoRunEnabled(autoRunKey(), autoRunPath());
}

void QnClientAutoRunWatcher::setAutoRun(bool enabled)
{
    nx::vms::utils::setAutoRunEnabled(autoRunKey(), autoRunPath(), enabled);
}

QString QnClientAutoRunWatcher::autoRunKey() const
{
    return qApp->applicationName();
}

QString QnClientAutoRunWatcher::autoRunPath() const
{
    const QFileInfo appLauncherFile = QnClientInstallationsManager::appLauncher();
    if (appLauncherFile.exists())
        return toWindowsRegistryFormat(appLauncherFile.canonicalFilePath());

    const QFileInfo miniLauncherFile = QnClientInstallationsManager::miniLauncher();
    if (miniLauncherFile.exists())
        return toWindowsRegistryFormat(miniLauncherFile.canonicalFilePath());

    return QString();
}
