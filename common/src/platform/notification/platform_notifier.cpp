#include "platform_notifier.h"

#include "notifier_win.h"
#include "generic_notifier.h"

QnPlatformNotifier *QnPlatformNotifier::newInstance(QObject *parent) {
#ifdef Q_OS_WIN
    return new QnWindowsNotifier(parent);
#else 
    return new QnGenericNotifier(parent);
#endif
}


