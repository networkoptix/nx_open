#include "home_ptz_controller.h"

#include <cassert>

#include <QtCore/QCoreApplication>

#include <api/resource_property_adaptor.h>

#include <utils/common/invocation_event.h>

#include "ptz_controller_pool.h"
#include "home_ptz_executor.h"

#define QN_NEW_PRESET_IS_HOME


QnHomePtzController::QnHomePtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_adaptor(new QnJsonResourcePropertyAdaptor<QnPtzObject>(baseController->resource(), lit("ptzHomeObject"), QnPtzObject(), this)),
    m_executor(new QnHomePtzExecutor(baseController))
{
    assert(qnPtzPool); /* Ptz pool must exist as it hosts executor thread. */
    assert(!baseController->hasCapabilities(Qn::AsynchronousPtzCapability)); // TODO: #Elric

    m_executor->moveToThread(qnPtzPool->executorThread());
    m_executor->setHomePosition(m_adaptor->value());
    m_executor->restart();
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
    if(!base_type::continuousMove(speed))
        return false;

    m_executor->restart();
    return true;
}

bool QnHomePtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(!base_type::absoluteMove(space, position, speed))
        return false;

    m_executor->restart();
    return true;
}

bool QnHomePtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    if(!base_type::viewportMove(aspectRatio, viewport, speed))
        return false;

    m_executor->restart();
    return true;
}

bool QnHomePtzController::createPreset(const QnPtzPreset &preset) {
    if(!base_type::createPreset(preset))
        return false;

#ifdef QN_NEW_PRESET_IS_HOME
    updateHomeObject(QnPtzObject(Qn::PresetPtzObject, preset.id));
#endif
    return true;
}

bool QnHomePtzController::activatePreset(const QString &presetId, qreal speed) {
    if(!base_type::activatePreset(presetId, speed))
        return false;

    m_executor->restart();
    return true;
}

bool QnHomePtzController::activateTour(const QString &tourId) {
    if(!base_type::activateTour(tourId))
        return false;

    m_executor->stop();
    return true;
}

bool QnHomePtzController::updateHomeObject(const QnPtzObject &homeObject) {
    Qn::PtzCapabilities capabilities = getCapabilities();
    if(homeObject.type == Qn::PresetPtzObject && !(capabilities & Qn::PresetsPtzCapability))
        return false;
    if(homeObject.type == Qn::TourPtzObject && !(capabilities & Qn::ToursPtzCapability))
        return false;

    QMutexLocker locker(&m_mutex);

    if(homeObject == m_adaptor->value())
        return true; /* Nothing to update. */
    m_adaptor->setValue(homeObject);

    m_executor->setHomePosition(homeObject);
    m_executor->restart();

    return true;
}

bool QnHomePtzController::getHomeObject(QnPtzObject *homeObject) {
    QMutexLocker locker(&m_mutex);

    *homeObject = m_adaptor->value();

    return true;
}

