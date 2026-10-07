// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tour_ptz_executor.h"

#include <algorithm>

#include <QtCore/QThread>
#include <QtCore/QTimerEvent>

#include <core/resource/camera_resource.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <nx/vms/common/system_context.h>

#include "threaded_ptz_controller.h"

using namespace nx::core;
using namespace nx::vms::common::ptz;
using namespace std::chrono;
using namespace std::chrono_literals;

QnTourPtzExecutor::SpotRuntimeData::SpotRuntimeData(): position(qQNaN<Vector>())
{
}

QnTourPtzExecutor::QnTourPtzExecutor(const QnPtzControllerPtr& controller, QThreadPool* threadPool)
{
    initializeController(controller, threadPool);

    connect(this,
        &QnTourPtzExecutor::startTourRequested,
        this,
        &QnTourPtzExecutor::startTourInternal,
        Qt::QueuedConnection);

    connect(this,
        &QnTourPtzExecutor::stopTourRequested,
        this,
        &QnTourPtzExecutor::stopTourInternal,
        Qt::QueuedConnection);

    connect(this,
        &QnTourPtzExecutor::controllerFinishedLater,
        this,
        &QnTourPtzExecutor::handleControllerFinished,
        Qt::QueuedConnection);
}

QnTourPtzExecutor::~QnTourPtzExecutor()
{
    // An executor running in a dedicated thread must be destroyed through deleteLater().
    NX_ASSERT(QThread::currentThread() == thread());

    // The threaded controller is owned both by a shared pointer and by QObject. Drop QObject
    // ownership before the shared pointer is destroyed to avoid deleting it twice.
    if (m_usesThreadedController)
        m_controller->setParent(nullptr);
}

void QnTourPtzExecutor::initializeController(
    const QnPtzControllerPtr& controller, QThreadPool* threadPool)
{
    m_controller = controller;

    if (m_controller->hasCapabilities(Ptz::Capability::asynchronous))
    {
        // Asynchronous controllers already satisfy the executor's event-loop contract.
    }
    else if (m_controller->hasCapabilities(Ptz::Capability::virtual_))
    {
        m_usesBlockingController = true;
    }
    else
    {
        m_controller.reset(new QnThreadedPtzController(m_controller, threadPool));
        m_usesThreadedController = true;

        // The threaded controller and tour executor both depend on the same event loop. Parenting
        // them also makes the controller follow the executor if it is moved after construction.
        m_controller->setParent(this);
    }

    connect(m_controller.get(),
        &QnAbstractPtzController::finished,
        this,
        &QnTourPtzExecutor::handleControllerFinished);

    const QnResourceData resourceData =
        controller->resource()->systemContext()->resourceDataPool()->data(
            m_controller->resource().dynamicCast<QnVirtualCameraResource>());
    m_tourGetPositionWorkaround = resourceData.value<bool>("tourGetPosWorkaround", false);
}

void QnTourPtzExecutor::updateControllerDefaults()
{
    m_positionSpace = m_controller->hasCapabilities(Ptz::Capability::logicalPositioning)
        ? CoordinateSpace::logical
        : CoordinateSpace::device;

    m_getPositionCommand = m_positionSpace == CoordinateSpace::logical
        ? Command::getLogicalPosition
        : Command::getDevicePosition;

    m_canReadPosition = m_controller->hasCapabilities(Ptz::Capability::devicePositioning)
        || m_controller->hasCapabilities(Ptz::Capability::logicalPositioning);
}

void QnTourPtzExecutor::startTour(const QnPtzTour& tour)
{
    emit startTourRequested(tour);
}

void QnTourPtzExecutor::stopTour()
{
    emit stopTourRequested();
}

void QnTourPtzExecutor::startTourInternal(const QnPtzTour& tour)
{
    stopTourInternal();

    NX_VERBOSE(this, "Start tour: %1", tour.name);
    m_tour.tour = tour;
    m_tour.tour.optimize();
    m_tour.spots.resize(m_tour.size());

    // Controller capabilities may change while the executor is alive.
    updateControllerDefaults();

    startMoving();
}

void QnTourPtzExecutor::stopTourInternal()
{
    NX_VERBOSE(this, "Stop tour: %1", m_tour.tour.name);
    m_state = State::stopped;

    m_moveTimer.stop();
    m_waitTimer.stop();
}

void QnTourPtzExecutor::startMoving()
{
    if (m_state == State::stopped)
    {
        m_spotIndex = 0;
        m_state = State::entering;
        m_lastPosition = qQNaN<Vector>();
        m_lastPositionRequestTime = 0ms;
        m_startPosition = qQNaN<Vector>();
    }
    else if (m_state == State::waiting)
    {
        m_spotIndex = (m_spotIndex + 1) % m_tour.size();
        m_state = State::moving;
        m_startPosition = m_lastPosition;
    }
    else
    {
        return;
    }

    NX_VERBOSE(this, "Go to spot: %1", m_spotIndex);
    m_spotTimer.restart();

    activateCurrentSpot();

    m_moveTimer.start(kPositionPollInterval, this);
    m_usingDefaultMoveTimer = true;
    m_spotActivationDelayPending = true;
}

void QnTourPtzExecutor::processMoving()
{
    if (m_state != State::entering && m_state != State::moving)
        return;

    if (!m_usingDefaultMoveTimer)
    {
        m_moveTimer.start(kPositionPollInterval, this);
        m_usingDefaultMoveTimer = true;
    }

    if (m_spotActivationDelayPending)
    {
        m_spotActivationDelayPending = false;
        handleSpotActivationDelay();
    }
    else if (m_positionRequestInProgress)
    {
        m_positionUpdatePending = true;
    }
    else
    {
        requestPosition();
    }
}

void QnTourPtzExecutor::processPosition(bool success, const Vector& position)
{
    if (m_state != State::entering && m_state != State::moving)
        return;

    NX_VERBOSE(this, "Got position: %1, status: %2", position, success);
    const bool moved = !qFuzzyEquals(m_startPosition, position);
    const bool stopped = qFuzzyEquals(m_lastPosition, position);

    if (success && stopped
        && (moved || milliseconds(m_spotTimer.elapsed()) > kSamePositionTimeout))
    {
        if (m_state == State::moving)
        {
            SpotRuntimeData& spotData = currentSpotData();
            spotData.moveTime = m_lastPositionRequestTime;
            if (m_tourGetPositionWorkaround && !qFuzzyEquals(spotData.position, m_lastPosition))
            {
                // VIVOTEK SD8363E stops briefly after getPosition(). Account for that observed
                // delay when scheduling subsequent visits to the same spot.
                spotData.moveTime += kPositionPollInterval;
                NX_DEBUG(this, "Increase spot move timeout to %1 ms", spotData.moveTime.count());
            }
            spotData.position = m_lastPosition;
        }

        m_repeatTimeout = kPositionPollInterval;
        m_moveTimer.stop();
        startWaiting();
    }
    else
    {
        if (success)
        {
            m_lastPosition = position;
            m_lastPositionRequestTime = m_newPositionRequestTime;
            m_repeatTimeout = kPositionPollInterval;
        }
        else
        {
            // Some cameras such as VIVOTEK SD9161 can lock down under frequent position requests.
            m_repeatTimeout =
                std::min(m_repeatTimeout * kRepeatTimeoutMultiplier, kMaxRepeatTimeout);
        }

        m_positionRequestInProgress = false;
        if (m_positionUpdatePending)
        {
            m_moveTimer.start(m_repeatTimeout, this);
            NX_VERBOSE(this, "Next get position in %1 ms", m_repeatTimeout.count());
        }
    }
}

void QnTourPtzExecutor::startWaiting()
{
    if (m_state != State::entering && m_state != State::moving)
        return;

    m_state = State::waiting;

    const auto waitTime = milliseconds(currentSpot().stayTime)
        - std::min(0ms, milliseconds(m_spotTimer.elapsed()) - currentSpotData().moveTime);
    if (waitTime > 0ms)
    {
        NX_VERBOSE(this, "Wait for: %1 ms", waitTime.count());
        m_waitTimer.start(waitTime, this);
    }
    else
    {
        processWaiting();
    }
}

void QnTourPtzExecutor::processWaiting()
{
    if (m_state != State::waiting)
        return;

    m_waitTimer.stop();
    startMoving();
}

void QnTourPtzExecutor::activateCurrentSpot()
{
    const QnPtzTourSpot& spot = currentSpot();
    m_controller->activatePreset(spot.presetId, spot.speed);
}

void QnTourPtzExecutor::requestPosition()
{
    if (!m_canReadPosition)
        return;

    Vector position;
    m_controller->getPosition(&position, m_positionSpace);

    m_positionUpdatePending = false;
    m_positionRequestInProgress = true;
    m_newPositionRequestTime = milliseconds(m_spotTimer.elapsed());

    if (m_usesBlockingController)
        emit controllerFinishedLater(m_getPositionCommand, QVariant::fromValue(position));
}

void QnTourPtzExecutor::handleControllerFinished(Command command, const QVariant& data)
{
    if (!m_canReadPosition && command == Command::activatePreset)
    {
        m_moveTimer.stop();
        startWaiting();
    }
    else if (command == m_getPositionCommand)
    {
        processPosition(data.isValid(), data.value<Vector>());
    }
}

void QnTourPtzExecutor::handleSpotActivationDelay()
{
    requestPosition();

    auto timeout = kPositionPollInterval;
    const SpotRuntimeData& spotData = currentSpotData();
    if (m_state == State::moving && spotData.moveTime > kPositionPollInterval * 2)
    {
        NX_VERBOSE(this, "Estimated move time: %1 ms", spotData.moveTime.count());
        timeout = spotData.moveTime - kPositionPollInterval * 2;
    }

    m_moveTimer.start(timeout, this);
    m_usingDefaultMoveTimer = timeout == kPositionPollInterval;
}

void QnTourPtzExecutor::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_moveTimer.timerId())
    {
        processMoving();
    }
    else if (event->timerId() == m_waitTimer.timerId())
    {
        processWaiting();
    }
    else
    {
        base_type::timerEvent(event);
    }
}
