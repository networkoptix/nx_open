#include "workbench_auto_starter.h"

#include <QSettings>

#include <client/client_settings.h>

#include "version.h"

namespace {
    const QString registryPath = lit("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}

QnWorkbenchAutoStarter::QnWorkbenchAutoStarter(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{

    if (settingsKey() < 0)
        return;

    /* Write out the setting first, then listen to changes. */
    qnSettings->setValue(settingsKey(), isAutoStartEnabled());
    connect(qnSettings->notifier(settingsKey()), &QnPropertyNotifier::valueChanged, this, &QnWorkbenchAutoStarter::at_autoStartSetting_valueChanged);
}

QnWorkbenchAutoStarter::~QnWorkbenchAutoStarter() {
    return;
}

bool QnWorkbenchAutoStarter::isSupported() const {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool QnWorkbenchAutoStarter::isAutoStartEnabled() {
#ifdef Q_OS_WIN
    QSettings settings(registryPath, QSettings::NativeFormat);

    QString realPath = autoStartPath();
    QString registryPath = fromRegistryFormat(settings.value(autoStartKey()).toString());
    
    return realPath == registryPath;
#else
    return false;
#endif
}

void QnWorkbenchAutoStarter::setAutoStartEnabled(bool autoStartEnabled) {
#ifdef Q_OS_WIN
    QSettings settings(registryPath, QSettings::NativeFormat);

    if(autoStartEnabled) {
        settings.setValue(autoStartKey(), toRegistryFormat(autoStartPath()));
    } else {
        settings.remove(autoStartKey());
    }
#else
    Q_UNUSED(autoStartEnabled);
#endif
}

void QnWorkbenchAutoStarter::at_autoStartSetting_valueChanged() {
    setAutoStartEnabled(qnSettings->value(settingsKey()).toBool());
}

int QnWorkbenchAutoStarter::settingsKey() const {
    return QnClientSettings::AUTO_START;
}

QString QnWorkbenchAutoStarter::autoStartKey() const {
    return lit(QN_APPLICATION_NAME);
}

QString QnWorkbenchAutoStarter::autoStartPath() const {
    QFileInfo clientFile = QFileInfo(qApp->applicationFilePath());
    QFileInfo launcherFile = QFileInfo(clientFile.dir(), lit(QN_APPLAUNCHER_EXECUTABLE_NAME));

    QFileInfo resultFile = launcherFile.exists() ? launcherFile : clientFile;
    return QDir::toNativeSeparators(resultFile.canonicalFilePath()).toLower();
}

QString QnWorkbenchAutoStarter::fromRegistryFormat(const QString &path) const {
    QString result = QDir::toNativeSeparators(path).toLower();
    result.replace(L'"', QString());
    return result;
}

QString QnWorkbenchAutoStarter::toRegistryFormat(const QString &path) const {
    return lit("\"") + QDir::toNativeSeparators(path).toLower() + lit("\"");
}
