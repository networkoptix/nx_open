#include "ptz_tour_executor.h"

#include <cassert>

#include <QtCore/QBasicTimer>

#include <utils/math/math.h>
#include <utils/common/invocation_event.h>

#include "abstract_ptz_controller.h"


// -------------------------------------------------------------------------- //
// QnPtzTourExecutorPrivate
// -------------------------------------------------------------------------- //
class QnPtzTourExecutorPrivate {
public:
    enum State {
        Stopped,
        Waiting,
        Moving,
    };

    QnPtzTourExecutorPrivate(): currentState(Stopped) {}

    void init(const QnPtzControllerPtr &controller);

    void stopTour();
    void startTour(const QnPtzTour &tour);

    void startMoving();
    void processMoving();
    void tryStartWaiting();
    void startWaiting();
    void processWaiting();

    bool handleTimer(int timerId);
    void handleSynchronized(Qn::PtzDataFields fields);

    QnPtzTourExecutor *q;

    QnPtzControllerPtr controller;
    Qn::PtzCoordinateSpace defaultSpace;
    Qn::PtzDataField defaultDataField;
    
    QBasicTimer moveTimer, waitTimer;

    QnPtzTour currentTour;
    int currentIndex;
    State currentState;
    QVector3D currentPosition;
};

void QnPtzTourExecutorPrivate::init(const QnPtzControllerPtr &controller) {
    this->controller = controller;
    defaultSpace = controller->hasCapabilities(Qn::LogicalPositioningPtzCapability) ? Qn::LogicalPtzCoordinateSpace : Qn::DevicePtzCoordinateSpace;
    defaultDataField = defaultSpace == Qn::LogicalPtzCoordinateSpace ? Qn::LogicalPositionPtzField : Qn::DevicePositionPtzField;

    QObject::connect(controller.data(), SIGNAL(synchronized(Qn::PtzDataFields)), q, SLOT(at_controller_synchronized(Qn::PtzDataFields)));
}

void QnPtzTourExecutorPrivate::stopTour() {
    currentState = Stopped;

    moveTimer.stop();
    waitTimer.stop();
}

void QnPtzTourExecutorPrivate::startTour(const QnPtzTour &tour) {
    stopTour();

    currentTour = tour;
    startMoving();
}

void QnPtzTourExecutorPrivate::startMoving() {
    assert(currentState == Stopped || currentState == Waiting);

    if(currentState == Stopped) {
        currentIndex = 0;
    } else {
        currentIndex = (currentIndex + 1) % currentTour.spots.size();
    }
    currentState = Moving;
    currentPosition = qQNaN<QVector3D>();

    qDebug() << "TOUR SPOT" << currentIndex;

    const QnPtzTourSpot &spot = currentTour.spots[currentIndex];

    controller->getPosition(defaultSpace, &currentPosition);
    controller->activatePreset(spot.presetId, spot.speed);
    moveTimer.start(1000, q);
}

void QnPtzTourExecutorPrivate::processMoving() {
    assert(currentState == Moving);

    controller->synchronize(defaultDataField);
}

void QnPtzTourExecutorPrivate::tryStartWaiting() {
    assert(currentState == Moving);

    QVector3D lastPosition = currentPosition;
    controller->getPosition(defaultSpace, &currentPosition);
    if(qFuzzyCompare(lastPosition, currentPosition)) {
        moveTimer.stop();
        startWaiting();
    }
}

void QnPtzTourExecutorPrivate::startWaiting() {
    assert(currentState == Moving);

    currentState = Waiting;

    int waitTime = currentTour.spots[currentIndex].stayTime;
    if(waitTime != 0) {
        waitTimer.start(waitTime, q);
    } else {
        processWaiting();
    }
}

void QnPtzTourExecutorPrivate::processWaiting() {
    assert(currentState == Waiting);

    waitTimer.stop();
    startMoving();
}

bool QnPtzTourExecutorPrivate::handleTimer(int timerId) {
    if(timerId == moveTimer.timerId()) {
        processMoving();
        return true;
    } else if(timerId == waitTimer.timerId()) {
        processWaiting();
        return true;
    } else {
        return false;
    }
}

void QnPtzTourExecutorPrivate::handleSynchronized(Qn::PtzDataFields fields) {
    if(currentState == Moving && (fields & defaultDataField))
        tryStartWaiting();
}


// -------------------------------------------------------------------------- //
// QnPtzTourExecutor
// -------------------------------------------------------------------------- //
QnPtzTourExecutor::QnPtzTourExecutor(const QnPtzControllerPtr &controller):
    d(new QnPtzTourExecutorPrivate())
{
    d->q = this;
    d->init(controller);

    connect(this, &QnPtzTourExecutor::startTourRequested,   this, &QnPtzTourExecutor::at_startTourRequested, Qt::QueuedConnection);
    connect(this, &QnPtzTourExecutor::stopTourRequested,    this, &QnPtzTourExecutor::at_stopTourRequested, Qt::QueuedConnection);
}

QnPtzTourExecutor::~QnPtzTourExecutor() {
    /* If this object is run in a separate thread, then it must be deleted with deleteLater(). */
    assert(QThread::currentThread() == thread()); 
}

void QnPtzTourExecutor::startTour(const QnPtzTour &tour) {
    emit startTourRequested(tour);
}

void QnPtzTourExecutor::stopTour() {
    emit stopTourRequested();
}

void QnPtzTourExecutor::timerEvent(QTimerEvent *event) {
    if(!d->handleTimer(event->timerId()))
        base_type::timerEvent(event);
}

void QnPtzTourExecutor::at_controller_synchronized(Qn::PtzDataFields fields) {
    d->handleSynchronized(fields);
}

void QnPtzTourExecutor::at_startTourRequested(const QnPtzTour &tour) {
    d->startTour(tour);
}

void QnPtzTourExecutor::at_stopTourRequested() {
    d->stopTour();
}
