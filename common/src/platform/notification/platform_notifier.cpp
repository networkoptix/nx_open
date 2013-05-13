#include "platform_notifier.h"

#ifdef Q_OS_WIN
#   include "notifier_win.h"
#else
#   include "generic_notifier.h"
#endif

QnPlatformNotifier *QnPlatformNotifier::newInstance(QObject *parent) {
#ifdef Q_OS_WIN
    return new QnWindowsNotifier(parent);
#else 
    return new QnGenericNotifier(parent);
#endif
}


