#include "workbench_auto_starter.h"

#include <QSettings>

#include <client/client_settings.h>

#include "version.h"

QnWorkbenchAutoStarter::QnWorkbenchAutoStarter(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    /* Write out the setting first, then listen to changes. */
    qnSettings->setAutoStart(isAutoStartEnabled());
    connect(qnSettings->notifier(QnClientSettings::AUTO_START), SIGNAL(valueChanged(int)), this, SLOT(at_autoStartSetting_valueChanged()));
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
    QSettings settings(lit("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"), QSettings::NativeFormat);

    QString realPath = autoStartPath();
    QString registryPath = fromRegistryFormat(settings.value(lit(QN_APPLICATION_NAME)).toString());
    
    return realPath == registryPath;
#else
    return false;
#endif
}

void QnWorkbenchAutoStarter::setAutoStartEnabled(bool autoStartEnabled) {
#ifdef Q_OS_WIN
    QSettings settings(lit("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"), QSettings::NativeFormat);

    if(autoStartEnabled) {
        settings.setValue(lit(QN_APPLICATION_NAME), toRegistryFormat(autoStartPath()));
    } else {
        settings.remove(lit(QN_APPLICATION_NAME));
    }
#else
    Q_UNUSED(autoStartEnabled);
#endif
}

void QnWorkbenchAutoStarter::at_autoStartSetting_valueChanged() {
    setAutoStartEnabled(qnSettings->autoStart());
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
