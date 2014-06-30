#include "tour_ptz_executor.h"

#include <cassert>

#include <QtCore/QBasicTimer>
#include <QtCore/QElapsedTimer>

#include <utils/math/math.h>
#include <utils/common/invocation_event.h>
#include <utils/common/connective.h>
#include <utils/common/collection.h>

#include "threaded_ptz_controller.h"

#define QN_TOUR_PTZ_EXECUTOR_DEBUG
#ifdef QN_TOUR_PTZ_EXECUTOR_DEBUG
#   define TRACE(...) qDebug() << "QnTourPtzExecutor:" << __VA_ARGS__;
#else
#   define TRACE(...)
#endif


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

    int size() const { return tour.spots.size(); }
};


// -------------------------------------------------------------------------- //
// QnTourPtzExecutorPrivate
// -------------------------------------------------------------------------- //
class QnTourPtzExecutorPrivate: public ConnectiveBase {
public:
    enum State {
        Stopped,
        Entering,
        Waiting,
        Moving,
    };

    QnTourPtzExecutorPrivate();
    virtual ~QnTourPtzExecutorPrivate();

    void init(const QnPtzControllerPtr &controller);
    void updateDefaults();

    void stopTour();
    void startTour(const QnPtzTour &tour);

    void startMoving();
    void processMoving();
    void processMoving(bool status, const QVector3D &position);
    void tryStartWaiting();
    void startWaiting();
    void processWaiting();

    void activateCurrentSpot();
    void requestPosition();

    bool handleTimer(int timerId);
    void handleFinished(Qn::PtzCommand command, const QVariant &data);

    QnPtzTourSpot &currentSpot() { return data.tour.spots[index]; }
    QnPtzTourSpotData &currentSpotData() { return data.spots[index]; }

    QnTourPtzExecutor *q;

    QnPtzControllerPtr baseController;
    bool usingThreadController;
    bool usingBlockingController;
    Qn::PtzCoordinateSpace defaultSpace;
    Qn::PtzDataField defaultDataField;
    Qn::PtzCommand defaultCommand;
    
    QBasicTimer moveTimer;
    QBasicTimer waitTimer;

    QnPtzTourData data;
    int index;
    State state;
    
    bool usingDefaultMoveTimer;
    bool needPositionUpdate;
    bool waitingForNewPosition;
    
    QElapsedTimer spotTimer;
    QVector3D startPosition;
    QVector3D lastPosition;
    int lastPositionRequestTime;
    int newPositionRequestTime;
};

QnTourPtzExecutorPrivate::QnTourPtzExecutorPrivate(): 
    q(nullptr),
    usingThreadController(false),
    usingBlockingController(false),
    index(-1),
    state(Stopped),
    usingDefaultMoveTimer(false),
    needPositionUpdate(false),
    waitingForNewPosition(false),
    lastPositionRequestTime(0),
    newPositionRequestTime(0)
{}

QnTourPtzExecutorPrivate::~QnTourPtzExecutorPrivate() {
    if(usingThreadController) {
        /* Base controller is owned both through a shared pointer and through
         * a QObject hierarchy. To prevent double deletion, we have to release 
         * QObject ownership. */
        baseController->setParent(NULL); 
    }
}

void QnTourPtzExecutorPrivate::init(const QnPtzControllerPtr &controller) {
    baseController = controller; 
    if(baseController->hasCapabilities(Qn::AsynchronousPtzCapability)) {
        /* Just use it as is. */
    } else if(baseController->hasCapabilities(Qn::VirtualPtzCapability)) {
        usingBlockingController = true;
    } else {
        baseController.reset(new QnThreadedPtzController(baseController));
        usingThreadController = true;

        /* This call makes sure that thread controller lives in the same thread
         * as tour executor. Both need an event loop to function properly,
         * and tour executor can be moved between threads after construction. */
        baseController->setParent(q); 
    }

    connect(baseController, &QnAbstractPtzController::finished, q, &QnTourPtzExecutor::at_controller_finished);
}

void QnTourPtzExecutorPrivate::updateDefaults() {
    defaultSpace = baseController->hasCapabilities(Qn::LogicalPositioningPtzCapability) ? Qn::LogicalPtzCoordinateSpace : Qn::DevicePtzCoordinateSpace;
    defaultDataField = defaultSpace == Qn::LogicalPtzCoordinateSpace ? Qn::LogicalPositionPtzField : Qn::DevicePositionPtzField;
    defaultCommand = defaultSpace == Qn::LogicalPtzCoordinateSpace ? Qn::GetLogicalPositionPtzCommand : Qn::GetDevicePositionPtzCommand;
}

void QnTourPtzExecutorPrivate::stopTour() {
    state = Stopped;

    moveTimer.stop();
    waitTimer.stop();
}

void QnTourPtzExecutorPrivate::startTour(const QnPtzTour &tour) {
    stopTour();

    TRACE("START TOUR" << tour.name);

    data.tour = tour;
    data.tour.optimize();
    data.space = defaultSpace;
    qnResizeList(data.spots, data.size());
    
    /* Capabilities of the underlying controller may have changed, 
     * and we don't listen to changes, so defaults must be updated. */
    updateDefaults(); 

    startMoving();
}

void QnTourPtzExecutorPrivate::startMoving() {
    if(state == Stopped) {
        index = 0;
        state = Entering;
        lastPosition = qQNaN<QVector3D>();
        lastPositionRequestTime = 0;
        
        startPosition = qQNaN<QVector3D>();
    } else if(state == Waiting) {
        index = (index + 1) % data.size();
        state = Moving;

        startPosition = lastPosition;
    } else {
        return; /* Invalid state. */
    }

    TRACE("GO TO SPOT" << index);

    spotTimer.restart();

    activateCurrentSpot();
    requestPosition();

    const QnPtzTourSpotData &spotData = currentSpotData();
    if(state == Moving && spotData.moveTime > pingTimeout) {
        TRACE("ESTIMATED MOVE TIME" << spotData.moveTime << "MS");

        moveTimer.start(spotData.moveTime - pingTimeout, q);
        usingDefaultMoveTimer = false;
    } else {
        moveTimer.start(pingTimeout, q);
        usingDefaultMoveTimer = true;
    }
}

void QnTourPtzExecutorPrivate::processMoving() {
    if(state != Entering && state != Moving)
        return;

    if(!usingDefaultMoveTimer) {
        moveTimer.start(pingTimeout, q);
        usingDefaultMoveTimer = true;
    }

    if(waitingForNewPosition) {
        needPositionUpdate = true;
    } else {
        requestPosition();
    }
}

void QnTourPtzExecutorPrivate::processMoving(bool status, const QVector3D &position) {
    if(state != Entering && state != Moving)
        return;

    TRACE("GOT POS" << position);

    bool moved = !qFuzzyEquals(startPosition, position);
    bool stopped = qFuzzyEquals(lastPosition, position);

    if(status && stopped && (moved || spotTimer.elapsed() > samePositionTimeout)) {
        if(state == Moving) {
            QnPtzTourSpotData &spotData = currentSpotData();
            spotData.moveTime = lastPositionRequestTime;
            spotData.position = lastPosition;
        }

        moveTimer.stop();
        startWaiting();
    } else {
        if(status) {
            lastPosition = position;
            lastPositionRequestTime = newPositionRequestTime;
        }

        if(needPositionUpdate) {
            requestPosition();
        } else {
            waitingForNewPosition = false;
        }
    }
}

void QnTourPtzExecutorPrivate::startWaiting() {
    if(state != Entering && state != Moving)
        return;

    state = Waiting;

    int waitTime = currentSpot().stayTime - qMin(0ll, spotTimer.elapsed() - currentSpotData().moveTime);
    if(waitTime > 0) {
        TRACE("WAIT FOR" << waitTime << "MS");

        waitTimer.start(waitTime, q);
    } else {
        processWaiting();
    }
}

void QnTourPtzExecutorPrivate::processWaiting() {
    if(state != Waiting)
        return;

    waitTimer.stop();
    startMoving();
}

void QnTourPtzExecutorPrivate::activateCurrentSpot() {
    const QnPtzTourSpot &spot = currentSpot();
    baseController->activatePreset(spot.presetId, spot.speed);
}

void QnTourPtzExecutorPrivate::requestPosition() {
    QVector3D position;
    baseController->getPosition(defaultSpace, &position);

    needPositionUpdate = false;
    waitingForNewPosition = true;

    newPositionRequestTime = spotTimer.elapsed();

    if(usingBlockingController)
        q->controllerFinishedLater(defaultCommand, position);
}

bool QnTourPtzExecutorPrivate::handleTimer(int timerId) {
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

void QnTourPtzExecutorPrivate::handleFinished(Qn::PtzCommand command, const QVariant &data) {
    if(command == defaultCommand)
        processMoving(data.isValid(), data.value<QVector3D>());
}


// -------------------------------------------------------------------------- //
// QnTourPtzExecutor
// -------------------------------------------------------------------------- //
QnTourPtzExecutor::QnTourPtzExecutor(const QnPtzControllerPtr &controller):
    d(new QnTourPtzExecutorPrivate())
{
    d->q = this;
    d->init(controller);

    connect(this, &QnTourPtzExecutor::startTourRequested,       this,   &QnTourPtzExecutor::at_startTourRequested,  Qt::QueuedConnection);
    connect(this, &QnTourPtzExecutor::stopTourRequested,        this,   &QnTourPtzExecutor::at_stopTourRequested,   Qt::QueuedConnection);
    connect(this, &QnTourPtzExecutor::controllerFinishedLater,  this,   &QnTourPtzExecutor::at_controller_finished, Qt::QueuedConnection);
}

QnTourPtzExecutor::~QnTourPtzExecutor() {
    /* If this object is run in a separate thread, then it must be deleted with deleteLater(). */
    assert(QThread::currentThread() == thread()); 
}

void QnTourPtzExecutor::startTour(const QnPtzTour &tour) {
    emit startTourRequested(tour);
}

void QnTourPtzExecutor::stopTour() {
    emit stopTourRequested();
}

void QnTourPtzExecutor::timerEvent(QTimerEvent *event) {
    if(!d->handleTimer(event->timerId()))
        base_type::timerEvent(event);
}

void QnTourPtzExecutor::at_controller_finished(Qn::PtzCommand command, const QVariant &data) {
    d->handleFinished(command, data);
}

void QnTourPtzExecutor::at_startTourRequested(const QnPtzTour &tour) {
    d->startTour(tour);
}

void QnTourPtzExecutor::at_stopTourRequested() {
    d->stopTour();
}
