#include "platform_notifier.h"

#include "windows_notifier.h"
#include "generic_notifier.h"

QnPlatformNotifier *QnPlatformNotifier::newInstance(QObject *parent) {
#ifdef Q_OS_WIN
    return new QnWindowsNotifier(parent);
#else 
    return new QnGenericNotifier(parent);
#endif
}


