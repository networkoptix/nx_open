#include "ptz_tour_executor.h"

#include <cassert>

#include <QtCore/QBasicTimer>
#include <QtCore/QElapsedTimer>

#include <utils/math/math.h>
#include <utils/common/invocation_event.h>
#include <utils/common/connective.h>
#include <utils/common/container.h>

#include "threaded_ptz_controller.h"

namespace {
    int pingTimeout = 333;
    int samePositionTimeout = 5000;
}


// -------------------------------------------------------------------------- //
// Model Data
// -------------------------------------------------------------------------- //
struct QnPtzTourSpotData {
    QnPtzTourSpotData(): position(qQNaN<QVector3D>()), moveTime(-1) {}

    QVector3D position;
    qint64 moveTime;
};

typedef QList<QnPtzTourSpotData> QnPtzTourSpotDataList;

struct QnPtzTourData {
    QnPtzTour tour;
    
    Qn::PtzCoordinateSpace space;
    QnPtzTourSpotDataList spots;
};


// -------------------------------------------------------------------------- //
// QnPtzTourExecutorPrivate
// -------------------------------------------------------------------------- //
class QnPtzTourExecutorPrivate: public ConnectiveBase {
public:
    enum State {
        Stopped,
        Entering,
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
    void processMoving(bool status, const QVector3D &position);
    void tryStartWaiting();
    void startWaiting();
    void processWaiting();

    bool handleTimer(int timerId);
    void handleFinished(Qn::PtzCommand command, const QVariant &data);

    QnPtzTourSpot &currentSpot() { return currentData.tour.spots[currentIndex]; }
    QnPtzTourSpotData &currentSpotData() { return currentData.spots[currentIndex]; }

    QnPtzTourExecutor *q;

    QnPtzControllerPtr baseController;
    bool usingThreadController;
    Qn::PtzCoordinateSpace defaultSpace;
    Qn::PtzDataField defaultDataField;
    Qn::PtzCommand defaultCommand;
    
    QBasicTimer moveTimer, waitTimer;

    QnPtzTourData currentData;
    int currentSize;
    int currentIndex;
    State currentState;
    
    bool usingDefaultMoveTimer;
    bool needPositionUpdate;
    bool waitingForNewPosition;
    
    QElapsedTimer spotTimer;
    QVector3D startPosition;
    QVector3D currentPosition;
};

QnPtzTourExecutorPrivate::QnPtzTourExecutorPrivate(): 
    currentState(Stopped),
    usingThreadController(false)
{}

QnPtzTourExecutorPrivate::~QnPtzTourExecutorPrivate() {
    /* It's important to release the QObject ownership here as thread controller 
     * is also owned by a QSharedPointer. */
    if(usingThreadController)
        baseController->setParent(NULL); 
}

void QnPtzTourExecutorPrivate::init(const QnPtzControllerPtr &controller) {
    baseController = controller; 
    if(QnThreadedPtzController::extends(baseController->getCapabilities())) {
        baseController.reset(new QnThreadedPtzController(baseController));
        usingThreadController = true;

        /* This call makes sure that thread controller lives in the same thread
         * as tour executor. Both need an event loop to function properly,
         * and tour executor can be moved between threads after construction. */
        baseController->setParent(q); 
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

    currentData.tour = tour;
    currentData.tour.optimize();
    currentData.space = defaultSpace;
    currentSize = currentData.tour.spots.size();
    qnResizeList(currentData.spots, currentSize);

    startMoving();
}

void QnPtzTourExecutorPrivate::startMoving() {
    if(currentState == Stopped) {
        currentIndex = 0;
        currentState = Entering;
        currentPosition = qQNaN<QVector3D>();
        
        startPosition = qQNaN<QVector3D>();
    } else if(currentState == Waiting) {
        currentIndex = (currentIndex + 1) % currentSize;
        currentState = Moving;

        startPosition = currentPosition;
    } else {
        return; /* Invalid state. */
    }

    //qDebug() << "TOUR SPOT" << currentIndex;

    spotTimer.restart();

    const QnPtzTourSpot &spot = currentSpot();
    const QnPtzTourSpotData &spotData = currentSpotData();
    
    QVector3D tmp;
    baseController->activatePreset(spot.presetId, spot.speed);
    baseController->getPosition(defaultSpace, &tmp);

    needPositionUpdate = false;
    waitingForNewPosition = true;

    if(currentState == Moving && spotData.moveTime > pingTimeout) {
        moveTimer.start(spotData.moveTime - pingTimeout, q);
        usingDefaultMoveTimer = false;
    } else {
        moveTimer.start(pingTimeout, q);
        usingDefaultMoveTimer = true;
    }
}

void QnPtzTourExecutorPrivate::processMoving() {
    if(currentState != Entering && currentState != Moving)
        return;

    if(!usingDefaultMoveTimer) {
        moveTimer.start(pingTimeout, q);
        usingDefaultMoveTimer = true;
    }

    if(waitingForNewPosition) {
        needPositionUpdate = true;
    } else {
        QVector3D tmp;
        baseController->getPosition(defaultSpace, &tmp);
        
        needPositionUpdate = false;
        waitingForNewPosition = true;
    }
}

void QnPtzTourExecutorPrivate::processMoving(bool status, const QVector3D &position) {
    if(currentState != Entering && currentState != Moving)
        return;

    //qDebug() << "GOT POS" << position;

    bool moved = !qFuzzyEquals(startPosition, position);
    bool stopped = qFuzzyEquals(currentPosition, position);

    if(status && stopped && (moved || spotTimer.elapsed() > samePositionTimeout)) {
        if(currentState == Moving) {
            QnPtzTourSpotData &spotData = currentSpotData();
            spotData.moveTime = spotTimer.elapsed();
            spotData.position = position;
        }

        moveTimer.stop();
        startWaiting();
    } else {
        if(status)
            currentPosition = position;

        if(needPositionUpdate) {
            QVector3D tmp;
            baseController->getPosition(defaultSpace, &tmp);

            waitingForNewPosition = true;
            needPositionUpdate = false;
        } else {
            waitingForNewPosition = false;
        }
    }
}

void QnPtzTourExecutorPrivate::startWaiting() {
    if(currentState != Entering && currentState != Moving)
        return;

    currentState = Waiting;

    int waitTime = currentSpot().stayTime;
    if(waitTime != 0) {
        waitTimer.start(waitTime, q);
    } else {
        processWaiting();
    }
}

void QnPtzTourExecutorPrivate::processWaiting() {
    if(currentState != Waiting)
        return;

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
    if(command == defaultCommand)
        processMoving(data.isValid(), data.value<QVector3D>());
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
