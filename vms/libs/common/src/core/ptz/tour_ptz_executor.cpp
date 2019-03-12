#include "tour_ptz_executor.h"

#include <cassert>

#include <QtCore/QBasicTimer>
#include <QtCore/QElapsedTimer>

#include <utils/math/math.h>
#include <utils/common/invocation_event.h>
#include <utils/common/connective.h>
#include <nx/utils/collection.h>
#include <nx/core/ptz/vector.h>
#include <nx/utils/log/log.h>

#include "threaded_ptz_controller.h"
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"
#include "core/resource/security_cam_resource.h"

using namespace nx::core;

namespace {
    int pingTimeout = 333;
    int samePositionTimeout = 5000;
}

// -------------------------------------------------------------------------- //
// Model Data
// -------------------------------------------------------------------------- //
struct QnPtzTourSpotData {
    QnPtzTourSpotData(): position(qQNaN<nx::core::ptz::Vector>()), moveTime(-1) {}

    nx::core::ptz::Vector position;
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
class QnTourPtzExecutorPrivate: public ConnectiveBase
{
public:
    enum State {
        Stopped,
        Entering,
        Waiting,
        Moving,
    };

    QnTourPtzExecutorPrivate();
    virtual ~QnTourPtzExecutorPrivate();

    void init(const QnPtzControllerPtr &controller, QThreadPool* threadPool);
    void updateDefaults();

    void stopTour();
    void startTour(const QnPtzTour &tour);

    void startMoving();
    void processMoving();
    void processMoving(bool status, const nx::core::ptz::Vector& position);
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
    nx::core::ptz::Vector startPosition;
    nx::core::ptz::Vector lastPosition;
    int lastPositionRequestTime;
    int newPositionRequestTime;
    bool tourGetPosWorkaround;
    bool canReadPosition;
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
    newPositionRequestTime(0),
    canReadPosition(false)
{}

QnTourPtzExecutorPrivate::~QnTourPtzExecutorPrivate() {
    if(usingThreadController) {
        /* Base controller is owned both through a shared pointer and through
         * a QObject hierarchy. To prevent double deletion, we have to release
         * QObject ownership. */
        baseController->setParent(NULL);
    }
}

void QnTourPtzExecutorPrivate::init(const QnPtzControllerPtr &controller, QThreadPool* threadPool)
{
    baseController = controller;

    if(baseController->hasCapabilities(Ptz::AsynchronousPtzCapability)){
        /* Just use it as is. */
    } else if(baseController->hasCapabilities(Ptz::VirtualPtzCapability)) {
        usingBlockingController = true;
    } else {
        baseController.reset(new QnThreadedPtzController(baseController, threadPool));
        usingThreadController = true;

        /* This call makes sure that thread controller lives in the same thread
         * as tour executor. Both need an event loop to function properly,
         * and tour executor can be moved between threads after construction. */
        baseController->setParent(q);
    }
    connect(baseController, &QnAbstractPtzController::finished, q, &QnTourPtzExecutor::at_controller_finished);
    QnResourceData resourceData = controller->resource()->commonModule()->resourceDataPool()
        ->data(baseController->resource().dynamicCast<QnSecurityCamResource>());
    tourGetPosWorkaround = resourceData.value<bool>(lit("tourGetPosWorkaround"), false);
}

void QnTourPtzExecutorPrivate::updateDefaults()
{
    defaultSpace =
        baseController->hasCapabilities(Ptz::LogicalPositioningPtzCapability)
            ? Qn::LogicalPtzCoordinateSpace
            : Qn::DevicePtzCoordinateSpace;

    defaultDataField = defaultSpace == Qn::LogicalPtzCoordinateSpace
        ? Qn::LogicalPositionPtzField
        : Qn::DevicePositionPtzField;

    defaultCommand = defaultSpace == Qn::LogicalPtzCoordinateSpace
        ? Qn::GetLogicalPositionPtzCommand
        : Qn::GetDevicePositionPtzCommand;

    canReadPosition =
        baseController->hasCapabilities(Ptz::DevicePositioningPtzCapability)
        || baseController->hasCapabilities(Ptz::LogicalPositioningPtzCapability);
}

void QnTourPtzExecutorPrivate::stopTour()
{
    NX_VERBOSE(this, "Stop tour: %1", data.tour.name);
    state = Stopped;

    moveTimer.stop();
    waitTimer.stop();
}

void QnTourPtzExecutorPrivate::startTour(const QnPtzTour &tour)
{
    stopTour();

    NX_VERBOSE(this, "Start tour: %1", tour.name);
    data.tour = tour;
    data.tour.optimize();
    data.space = defaultSpace;
    qnResizeList(data.spots, data.size());

    /* Capabilities of the underlying controller may have changed,
     * and we don't listen to changes, so defaults must be updated. */
    updateDefaults();

    startMoving();
}

void QnTourPtzExecutorPrivate::startMoving()
{
    if(state == Stopped) {
        index = 0;
        state = Entering;
        lastPosition = qQNaN<nx::core::ptz::Vector>();
        lastPositionRequestTime = 0;

        startPosition = qQNaN<nx::core::ptz::Vector>();
    } else if(state == Waiting) {
        index = (index + 1) % data.size();
        state = Moving;

        startPosition = lastPosition;
    } else {
        return; /* Invalid state. */
    }

    NX_VERBOSE(this, "Go to spot: %1", index);
    spotTimer.restart();

    activateCurrentSpot();
    requestPosition();

    const QnPtzTourSpotData &spotData = currentSpotData();
    if(state == Moving && spotData.moveTime > pingTimeout) {
        NX_VERBOSE(this, "Estimated move time: %1 ms", spotData.moveTime);
        moveTimer.start(spotData.moveTime - pingTimeout, q);
        usingDefaultMoveTimer = false;
    } else {
        moveTimer.start(pingTimeout, q);
        usingDefaultMoveTimer = true;
    }
}

void QnTourPtzExecutorPrivate::processMoving()
{
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

void QnTourPtzExecutorPrivate::processMoving(bool status, const nx::core::ptz::Vector& position)
{
    if(state != Entering && state != Moving)
        return;

    NX_VERBOSE(this, "Got position: %1", position);
    bool moved = !qFuzzyEquals(startPosition, position);
    bool stopped = qFuzzyEquals(lastPosition, position);

    if(status && stopped && (moved || spotTimer.elapsed() > samePositionTimeout)) {
        if(state == Moving) {
            QnPtzTourSpotData &spotData = currentSpotData();
            spotData.moveTime = lastPositionRequestTime;
            if (tourGetPosWorkaround && !qFuzzyEquals(spotData.position, lastPosition))
                spotData.moveTime += pingTimeout; // workaround for VIVOTEK SD8363E camera. It stops after getPosition call. So, increase getPosition timeout if we detect that camera changes position.
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
        NX_VERBOSE(this, "Wait for: %1 ms", waitTime);
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

void QnTourPtzExecutorPrivate::requestPosition()
{
    if (!canReadPosition)
        return;

    nx::core::ptz::Vector position;
    baseController->getPosition(defaultSpace, &position);

    needPositionUpdate = false;
    waitingForNewPosition = true;

    newPositionRequestTime = spotTimer.elapsed();

    if(usingBlockingController)
        q->controllerFinishedLater(defaultCommand, QVariant::fromValue(position));
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

void QnTourPtzExecutorPrivate::handleFinished(Qn::PtzCommand command, const QVariant &data)
{
    if (!canReadPosition && command == Qn::ActivatePresetPtzCommand) {
        moveTimer.stop();
        startWaiting();
    }
    else if(command == defaultCommand)
        processMoving(data.isValid(), data.value<nx::core::ptz::Vector>());
}

// -------------------------------------------------------------------------- //
// QnTourPtzExecutor
// -------------------------------------------------------------------------- //
QnTourPtzExecutor::QnTourPtzExecutor(const QnPtzControllerPtr &controller, QThreadPool* threadPool):
    d(new QnTourPtzExecutorPrivate())
{
    d->q = this;
    d->init(controller, threadPool);

    connect(
        this, &QnTourPtzExecutor::startTourRequested,
        this, &QnTourPtzExecutor::at_startTourRequested,
        Qt::QueuedConnection);

    connect(
        this, &QnTourPtzExecutor::stopTourRequested,
        this, &QnTourPtzExecutor::at_stopTourRequested,
        Qt::QueuedConnection);

    connect(
        this, &QnTourPtzExecutor::controllerFinishedLater,
        this, &QnTourPtzExecutor::at_controller_finished,
        Qt::QueuedConnection);
}

QnTourPtzExecutor::~QnTourPtzExecutor() {
    /* If this object is run in a separate thread, then it must be deleted with deleteLater(). */
    NX_ASSERT(QThread::currentThread() == thread());
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
