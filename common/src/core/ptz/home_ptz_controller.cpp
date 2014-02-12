#include "home_ptz_controller.h"

#include <cassert>

#include <QtCore/QCoreApplication>

#include <api/resource_property_adaptor.h>

#include <utils/common/invocation_event.h>

#include "ptz_controller_pool.h"
#include "home_ptz_executor.h"


QnHomePtzController::QnHomePtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_adaptor(new QnJsonResourcePropertyAdaptor<QnPtzObject>(baseController->resource(), lit("ptzHomePosition"), QnPtzObject(), this)),
    m_executor(new QnHomePtzExecutor(baseController))
{
    assert(qnPtzPool); /* Ptz pool must exist as it hosts executor thread. */

    m_executor->moveToThread(qnPtzPool->executorThread());
    m_executor->setHomePosition(m_adaptor->value());
    m_executor->restart();

    assert(!baseController->hasCapabilities(Qn::AsynchronousPtzCapability)); // TODO: #Elric
}

QnHomePtzController::~QnHomePtzController() {
    m_executor->deleteLater();
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

    m_executor->restart();
    return base_type::continuousMove(speed);
}

bool QnHomePtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(!supports(spaceCommand(Qn::AbsoluteDeviceMovePtzCommand, space)))
        return false;

    m_executor->restart();
    return base_type::absoluteMove(space, position, speed);
}

bool QnHomePtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    if(!supports(Qn::ViewportMovePtzCommand))
        return false;

    m_executor->restart();
    return base_type::viewportMove(aspectRatio, viewport, speed);
}

bool QnHomePtzController::activatePreset(const QString &presetId, qreal speed) {
    if(!supports(Qn::ActivatePresetPtzCommand))
        return false;

    m_executor->restart();
    return base_type::activatePreset(presetId, speed);
}

bool QnHomePtzController::activateTour(const QString &tourId) {
    if(!supports(Qn::ActivateTourPtzCommand))
        return false;

    m_executor->stop();
    return base_type::activateTour(tourId);
}

bool QnHomePtzController::updateHomePosition(const QnPtzObject &homePosition) {
    QMutexLocker locker(&m_mutex);

    if(homePosition == m_adaptor->value())
        return true; /* Nothing to update. */
    m_adaptor->setValue(homePosition);

    m_executor->setHomePosition(homePosition);
    m_executor->restart();

    return true;
}

bool QnHomePtzController::getHomePosition(QnPtzObject *homePosition) {
    QMutexLocker locker(&m_mutex);

    *homePosition = m_adaptor->value();

    return true;
}

