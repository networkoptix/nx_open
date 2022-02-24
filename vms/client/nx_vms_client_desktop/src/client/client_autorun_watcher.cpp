// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_autorun_watcher.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>

#include <client/client_settings.h>
#include <client/client_installations_manager.h>

#include <nx/vms/utils/platform/autorun.h>

#include <nx/branding.h>

namespace {

const int kSettingsKey = QnClientSettings::AUTO_START;

QString toWindowsRegistryFormat(const QString& path)
{
    return '"' + QDir::toNativeSeparators(path).toLower() + '"';
}

} // namespace

QnClientAutoRunWatcher::QnClientAutoRunWatcher(QObject* parent)
{
    // Write out the setting first, then listen to changes.
    bool autoRunEnabled = isAutoRun();
    qnSettings->setAutoStart(autoRunEnabled);

    if (autoRunEnabled)
    {
        // Fix path after migration from previous version.
        const QString path = nx::vms::utils::autoRunPath(autoRunKey());
        if (path != autoRunPath())
            nx::vms::utils::setAutoRunEnabled(autoRunKey(), autoRunPath(), true);
    }

    connect(qnSettings->notifier(kSettingsKey), &QnPropertyNotifier::valueChanged, this,
        [this]()
        {
            setAutoRun(qnSettings->autoStart());
        });
}

bool QnClientAutoRunWatcher::isAutoRun() const
{
    return nx::vms::utils::isAutoRunEnabled(autoRunKey());
}

void QnClientAutoRunWatcher::setAutoRun(bool enabled)
{
    nx::vms::utils::setAutoRunEnabled(autoRunKey(), autoRunPath(), enabled);
}

QString QnClientAutoRunWatcher::autoRunKey() const
{
    return nx::branding::desktopClientDisplayName();
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
