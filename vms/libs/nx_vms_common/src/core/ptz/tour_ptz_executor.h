// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include "ptz_fwd.h"

class QThreadPool;

class QnTourPtzExecutorPrivate;

/**
 * A controller that runs a PTZ tour on a given PTZ controller. Note that it
 * uses event loop and timers, and thus must be run in a thread with event loop,
 * better an dedicated thread.
 *
 * Public functions of this class are thread-safe.
 */
class QnTourPtzExecutor: public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnTourPtzExecutor(const QnPtzControllerPtr &controller, QThreadPool* threadPool);
    virtual ~QnTourPtzExecutor() override;

    void startTour(const QnPtzTour &tour);
    void stopTour();

protected:
    virtual void timerEvent(QTimerEvent *event) override;

private:
    friend class QnTourPtzExecutorPrivate;

    Q_SIGNAL void startTourRequested(const QnPtzTour &tour);
    Q_SIGNAL void stopTourRequested();
    Q_SIGNAL void controllerFinishedLater(
        nx::vms::common::ptz::Command command, const QVariant& data);

    void at_controller_finished(nx::vms::common::ptz::Command command, const QVariant& data);
    void at_startTourRequested(const QnPtzTour &tour);
    void at_stopTourRequested();

private:
    QScopedPointer<QnTourPtzExecutorPrivate> d;
};
