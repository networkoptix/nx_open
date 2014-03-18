#include "home_ptz_executor.h"

#include <QtCore/QMutex>

#include "abstract_ptz_controller.h"
#include "ptz_object.h"

namespace {
    int homeTimeout = 2 * 60 * 1000;
    //int homeTimeout = 10 * 1000;
}

// -------------------------------------------------------------------------- //
// QnHomePtzExecutorPrivate
// -------------------------------------------------------------------------- //
class QnHomePtzExecutorPrivate {
public:
    QnHomePtzExecutorPrivate(): q(NULL) {}
    virtual ~QnHomePtzExecutorPrivate() {}

    void restart();
    void stop();
    bool handleTimer(int timerId);

public:
    QnHomePtzExecutor *q;
    QnPtzControllerPtr controller;
    QBasicTimer timer;

    QMutex mutex;
    QnPtzObject homePosition;
};

void QnHomePtzExecutorPrivate::restart() {
    timer.start(homeTimeout, q);
}

void QnHomePtzExecutorPrivate::stop() {
    timer.stop();
}

bool QnHomePtzExecutorPrivate::handleTimer(int timerId) {
    if(timerId != timer.timerId()) 
        return false;
    timer.stop();

    QnPtzObject homePosition;
    {
        QMutexLocker locker(&mutex);
        homePosition = this->homePosition;
    }

    /* Note that we don't use threaded PTZ controller here as 
     * the activation commands are pretty rare. */

    if(homePosition.type == Qn::PresetPtzObject) {
        controller->activatePreset(homePosition.id, 1.0);
    } else if(homePosition.type == Qn::TourPtzObject) {
        controller->activateTour(homePosition.id);
    }

    return true;
}


// -------------------------------------------------------------------------- //
// QnHomePtzExecutor
// -------------------------------------------------------------------------- //
QnHomePtzExecutor::QnHomePtzExecutor(const QnPtzControllerPtr &controller):
    d(new QnHomePtzExecutorPrivate())
{
    d->q = this;
    d->controller = controller;

    connect(this, &QnHomePtzExecutor::restartRequested, this, &QnHomePtzExecutor::at_restartRequested, Qt::QueuedConnection);
    connect(this, &QnHomePtzExecutor::stopRequested,    this, &QnHomePtzExecutor::at_stopRequested, Qt::QueuedConnection);
}

QnHomePtzExecutor::~QnHomePtzExecutor() {
    /* If this object is run in a separate thread, then it must be deleted with deleteLater(). */
    assert(QThread::currentThread() == thread()); 
}

void QnHomePtzExecutor::restart() {
    emit restartRequested();
}

void QnHomePtzExecutor::stop() {
    emit stopRequested();
}

void QnHomePtzExecutor::setHomePosition(const QnPtzObject &homePosition) {
    QMutexLocker locker(&d->mutex);

    d->homePosition = homePosition;
}

QnPtzObject QnHomePtzExecutor::homePosition() const {
    QMutexLocker locker(&d->mutex);

    return d->homePosition;
}

void QnHomePtzExecutor::timerEvent(QTimerEvent *event) {
    if(!d->handleTimer(event->timerId()))
        base_type::timerEvent(event);
}

void QnHomePtzExecutor::at_restartRequested() {
    d->restart();
}

void QnHomePtzExecutor::at_stopRequested() {
    d->stop();
}
