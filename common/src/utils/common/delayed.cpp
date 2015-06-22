#include "delayed.h"

#include <QtCore/QTimer>

void executeDelayed(std::function<void()> callback, int delayMs, QThread *targetThread) {
    QTimer *timer = new QTimer();
    timer->setInterval(delayMs);
    timer->setSingleShot(true);

    if (targetThread)
        timer->moveToThread(targetThread);

    /* Set timer as context so if timer is destroyed, callback is also destroyed. */
    QObject::connect(timer, &QTimer::timeout, timer, callback);
    QObject::connect(timer, &QTimer::timeout, timer, &QObject::deleteLater);

    /* Workaround for windows. QTimer cannot be started from a different thread in windows. */
    QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection);
}
