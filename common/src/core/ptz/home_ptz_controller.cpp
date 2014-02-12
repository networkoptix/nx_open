#include "home_ptz_controller.h"

#include <cassert>

#include <QtCore/QCoreApplication>

#include <api/resource_property_adaptor.h>

#include <utils/common/invocation_event.h>

#include "ptz_controller_pool.h"


class QnHomePositionResourcePropertyAdaptor: public QnJsonResourcePropertyAdaptor<QnPtzObject> {
    typedef QnJsonResourcePropertyAdaptor<QnPtzObject> base_type;
public:
    QnHomePositionResourcePropertyAdaptor(const QnResourcePtr &resource, QObject *parent = NULL): 
        base_type(resource, lit("ptzHomePosition"), QnPtzObject(), parent) 
    {}
};


class QnHomePtzExecutor: public QObject {
    typedef QObject base_type;
public:
    enum {
        RestartTimerInvocation,
        StopTimerInvocation
    };

    QnHomePtzExecutor(const QnPtzControllerPtr &controller): m_controller(controller) {}

    void restartTimer() {
        QCoreApplication::postEvent(this, new QnInvocationEvent(RestartTimerInvocation));
    }

    void stopTimer() {
        QCoreApplication::postEvent(this, new QnInvocationEvent(StopTimerInvocation));
    }

protected:
    virtual bool event(QEvent *event) override {
        if(event->type() == QnEvent::Invocation) {
            invocationEvent(static_cast<QnInvocationEvent *>(event));
            return true;
        } else {
            return base_type::event(event);
        }
    }

    void invocationEvent(QnInvocationEvent *event) {
        if(event->type() == RestartTimerInvocation) {

        }
    }

private:
    QnPtzControllerPtr m_controller;
};


QnHomePtzController::QnHomePtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_adaptor(new QnHomePositionResourcePropertyAdaptor(baseController->resource(), this))
{
    assert(qnPtzPool); /* Ptz pool must exist as we're going to use executor thread. */

    assert(!baseController->hasCapabilities(Qn::AsynchronousPtzCapability)); // TODO: #Elric
}

QnHomePtzController::~QnHomePtzController() {
    return;
}

bool QnHomePtzController::extends(Qn::PtzCapabilities capabilities) {
    return 
        (capabilities & (Qn::PresetsPtzCapability | Qn::ToursPtzCapability)) &&
        !(capabilities & Qn::HomePtzCapability);
}

Qn::PtzCapabilities QnHomePtzController::getCapabilities() {
    Qn::PtzCapabilities capabilities = base_type::getCapabilities();
    return extends(capabilities) ? (capabilities | Qn::HomePtzCapability) : capabilities;
}

bool QnHomePtzController::continuousMove(const QVector3D &speed) {
    if(!supports(Qn::ContinuousMovePtzCommand))
        return false;

    restartTimer();
    return base_type::continuousMove(speed);
}

bool QnHomePtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(!supports(spaceCommand(Qn::AbsoluteDeviceMovePtzCommand, space)))
        return false;

    restartTimer();
    return base_type::absoluteMove(space, position, speed);
}

bool QnHomePtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    if(!supports(Qn::ViewportMovePtzCommand))
        return false;

    restartTimer();
    return base_type::viewportMove(aspectRatio, viewport, speed);
}

bool QnHomePtzController::activatePreset(const QString &presetId, qreal speed) {
    if(!supports(Qn::ActivatePresetPtzCommand))
        return false;

    restartTimer();
    return base_type::activatePreset(presetId, speed);
}

bool QnHomePtzController::activateTour(const QString &tourId) {
    if(!supports(Qn::ActivateTourPtzCommand))
        return false;

    stopTimer();
    return base_type::activateTour(tourId);
}

bool QnHomePtzController::updateHomePosition(const QnPtzObject &homePosition) {
    restartTimer();

    return true;
}

bool QnHomePtzController::getHomePosition(QnPtzObject *homePosition) {
    return true;
}

