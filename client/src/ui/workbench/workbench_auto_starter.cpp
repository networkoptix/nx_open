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

    QString realPath = QDir::toNativeSeparators(qApp->applicationFilePath()).toLower();
    QString registryPath = QDir::toNativeSeparators(settings.value(lit(QN_APPLICATION_NAME)).toString()).toLower();
    registryPath.replace(lit('"'), QString());
    
    return realPath == registryPath;
#else
    return false;
#endif
}

void QnWorkbenchAutoStarter::setAutoStartEnabled(bool autoStartEnabled) {
#ifdef Q_OS_WIN
    QSettings settings(lit("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"), QSettings::NativeFormat);

    if(autoStartEnabled) {
        settings.setValue(lit(QN_APPLICATION_NAME), lit("\"") + QDir::toNativeSeparators(qApp->applicationFilePath()).toLower() + lit("\""));
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
