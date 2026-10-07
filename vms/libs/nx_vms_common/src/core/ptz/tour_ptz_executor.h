// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QBasicTimer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include <nx/vms/common/ptz/command.h>
#include <nx/vms/common/ptz/coordinate_space.h>
#include <nx/vms/common/ptz/vector.h>

#include "ptz_fwd.h"
#include "ptz_tour.h"

class QThreadPool;

/**
 * Runs a PTZ tour using the supplied controller.
 *
 * The executor uses an event loop and timers, so it must live in a thread with an event loop.
 * Its public functions are thread-safe: commands are delivered to the executor through queued
 * connections.
 */
class QnTourPtzExecutor: public QObject
{
    Q_OBJECT
    using base_type = QObject;
    using Command = nx::vms::common::ptz::Command;
    using CoordinateSpace = nx::vms::common::ptz::CoordinateSpace;
    using Vector = nx::vms::common::ptz::Vector;

public:
    QnTourPtzExecutor(const QnPtzControllerPtr& controller, QThreadPool* threadPool);
    virtual ~QnTourPtzExecutor() override;

    void startTour(const QnPtzTour& tour);
    void stopTour();

protected:
    virtual void timerEvent(QTimerEvent* event) override;

private:
    enum class State
    {
        stopped,
        entering,
        waiting,
        moving,
    };

    struct SpotRuntimeData
    {
        SpotRuntimeData();

        Vector position;
        std::chrono::milliseconds moveTime{-1};
    };

    struct TourRuntimeData
    {
        int size() const { return tour.spots.size(); }

        QnPtzTour tour;
        QList<SpotRuntimeData> spots;
    };

    static constexpr std::chrono::milliseconds kPositionPollInterval{333};
    static constexpr std::chrono::milliseconds kSamePositionTimeout{5'000};
    static constexpr int kRepeatTimeoutMultiplier = 3;
    static constexpr std::chrono::milliseconds kMaxRepeatTimeout{10'000};

    Q_SIGNAL void startTourRequested(const QnPtzTour& tour);
    Q_SIGNAL void stopTourRequested();
    Q_SIGNAL void controllerFinishedLater(
        nx::vms::common::ptz::Command command, const QVariant& data);

    void initializeController(const QnPtzControllerPtr& controller, QThreadPool* threadPool);
    void updateControllerDefaults();

    void startTourInternal(const QnPtzTour& tour);
    void stopTourInternal();

    void startMoving();
    void processMoving();
    void processPosition(bool success, const Vector& position);
    void startWaiting();
    void processWaiting();

    void activateCurrentSpot();
    void requestPosition();
    void handleControllerFinished(Command command, const QVariant& data);
    void handleSpotActivationDelay();

    QnPtzTourSpot& currentSpot() { return m_tour.tour.spots[m_spotIndex]; }
    SpotRuntimeData& currentSpotData() { return m_tour.spots[m_spotIndex]; }

private:
    QnPtzControllerPtr m_controller;
    bool m_usesThreadedController = false;
    bool m_usesBlockingController = false;
    CoordinateSpace m_positionSpace = CoordinateSpace::device;
    Command m_getPositionCommand = Command::getDevicePosition;

    QBasicTimer m_moveTimer;
    QBasicTimer m_waitTimer;

    TourRuntimeData m_tour;
    int m_spotIndex = -1;
    State m_state = State::stopped;

    bool m_usingDefaultMoveTimer = false;
    bool m_positionUpdatePending = false;
    bool m_positionRequestInProgress = false;
    bool m_spotActivationDelayPending = false;

    QElapsedTimer m_spotTimer;
    Vector m_startPosition;
    Vector m_lastPosition;
    std::chrono::milliseconds m_lastPositionRequestTime{0};
    std::chrono::milliseconds m_newPositionRequestTime{0};
    bool m_tourGetPositionWorkaround = false;
    bool m_canReadPosition = false;
    std::chrono::milliseconds m_repeatTimeout = kPositionPollInterval;
};
