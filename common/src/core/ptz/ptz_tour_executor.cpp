#include "ptz_tour_executor.h"

#include <cassert>

#include <QtCore/QBasicTimer>
#include <QtCore/QElapsedTimer>

#include <utils/math/math.h>
#include <utils/common/invocation_event.h>
#include <utils/common/connective.h>

#include "threaded_ptz_controller.h"

// -------------------------------------------------------------------------- //
// Model Data
// -------------------------------------------------------------------------- //


struct QnPtzTourSpotData {
    QVector3D position;
    qint64 moveTime;
};

typedef QList<QnPtzTourSpotData> QnPtzTourSpotDataList;

struct QnPtzTourData {
    Qn::PtzCoordinateSpace space;
    QnPtzTourSpotDataList spots;
    uint hash;
};


// -------------------------------------------------------------------------- //
// QnPtzTourExecutorPrivate
// -------------------------------------------------------------------------- //
class QnPtzTourExecutorPrivate: public ConnectiveBase {
public:
    enum State {
        Stopped,
        Waiting,
        Moving,
    };

    QnPtzTourExecutorPrivate();
    virtual ~QnPtzTourExecutorPrivate();

    void init(const QnPtzControllerPtr &controller);

    void stopTour();
    void startTour(const QnPtzTour &tour);

    void startMoving();
    void processMoving();
    void processMoving(const QVector3D &position);
    void tryStartWaiting();
    void startWaiting();
    void processWaiting();

    bool handleTimer(int timerId);
    void handleFinished(Qn::PtzCommand command, const QVariant &data);

    QnPtzTourExecutor *q;

    QnPtzControllerPtr baseController;
    QnPtzControllerPtr threadController;
    Qn::PtzCoordinateSpace defaultSpace;
    Qn::PtzDataField defaultDataField;
    Qn::PtzCommand defaultCommand;
    
    QBasicTimer moveTimer, waitTimer;

    QnPtzTour currentTour;
    int currentIndex;
    State currentState;
    
    QElapsedTimer spotTimer;
    QVector3D startPosition;
    QVector3D currentPosition;
};

QnPtzTourExecutorPrivate::QnPtzTourExecutorPrivate(): 
    currentState(Stopped) 
{}

QnPtzTourExecutorPrivate::~QnPtzTourExecutorPrivate() {
    /* It's important to release the QObject ownership here as thread controller 
     * is also owned by a QSharedPointer. */
    if(threadController)
        threadController->setParent(NULL); 
}

void QnPtzTourExecutorPrivate::init(const QnPtzControllerPtr &controller) {
    baseController = controller;
    if(QnThreadedPtzController::extends(baseController)) {
        threadController.reset(new QnThreadedPtzController(baseController));
        baseController = threadController;

        /* This call makes sure that thread controller lives in the same thread
         * as tour executor. Both need an event loop to function properly,
         * and tour executor can be moved between threads after construction. */
        threadController->setParent(q); 
    }

    defaultSpace = baseController->hasCapabilities(Qn::LogicalPositioningPtzCapability) ? Qn::LogicalPtzCoordinateSpace : Qn::DevicePtzCoordinateSpace;
    defaultDataField = defaultSpace == Qn::LogicalPtzCoordinateSpace ? Qn::LogicalPositionPtzField : Qn::DevicePositionPtzField;
    defaultCommand = defaultSpace == Qn::LogicalPtzCoordinateSpace ? Qn::GetLogicalPositionPtzCommand : Qn::GetDevicePositionPtzCommand;

    connect(baseController, &QnAbstractPtzController::finished, q, &QnPtzTourExecutor::at_controller_finished);
}

void QnPtzTourExecutorPrivate::stopTour() {
    currentState = Stopped;

    moveTimer.stop();
    waitTimer.stop();
}

void QnPtzTourExecutorPrivate::startTour(const QnPtzTour &tour) {
    stopTour();

    currentTour = tour;
    currentTour.optimize();

    startMoving();
}

void QnPtzTourExecutorPrivate::startMoving() {
    assert(currentState == Stopped || currentState == Waiting);

    if(currentState == Stopped) {
        currentIndex = 0;
        currentPosition = qQNaN<QVector3D>();
        startPosition = qQNaN<QVector3D>();
    } else {
        currentIndex = (currentIndex + 1) % currentTour.spots.size();
        startPosition = currentPosition;
    }
    currentState = Moving;
    spotTimer.restart();

    qDebug() << "TOUR SPOT" << currentIndex;

    const QnPtzTourSpot &spot = currentTour.spots[currentIndex];

    QVector3D tmp;
    baseController->activatePreset(spot.presetId, spot.speed);
    baseController->getPosition(defaultSpace, &tmp);
    moveTimer.start(200, q);
}

void QnPtzTourExecutorPrivate::processMoving() {
    assert(currentState == Moving);

    QVector3D tmp;
    baseController->getPosition(defaultSpace, &tmp);
}

void QnPtzTourExecutorPrivate::processMoving(const QVector3D &position) {
    assert(currentState == Moving);

    if(qFuzzyCompare(currentPosition, position) && !qFuzzyCompare(currentPosition, startPosition)) {
        moveTimer.stop();
        startWaiting();
    } else {
        currentPosition = position;
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

void QnPtzTourExecutorPrivate::handleFinished(Qn::PtzCommand command, const QVariant &data) {
    if(command == defaultCommand && data.isValid())
        processMoving(data.value<QVector3D>());
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

void QnPtzTourExecutor::at_controller_finished(Qn::PtzCommand command, const QVariant &data) {
    d->handleFinished(command, data);
}

void QnPtzTourExecutor::at_startTourRequested(const QnPtzTour &tour) {
    d->startTour(tour);
}

void QnPtzTourExecutor::at_stopTourRequested() {
    d->stopTour();
}
