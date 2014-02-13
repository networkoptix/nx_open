#include "activity_ptz_controller.h"

#include <cassert>

#include <api/resource_property_adaptor.h>

QnActivityPtzController::QnActivityPtzController(bool isLocal, const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_isLocal(isLocal),
    m_adaptor(NULL)
{
    if(!m_isLocal) {
        m_adaptor = new QnJsonResourcePropertyAdaptor<QnPtzObject>(baseController->resource(), lit("ptzActiveObject"), QnPtzObject(), this);
        m_adaptor->setValue(QnPtzObject());
        connect(m_adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged, this, [this]{ emit changed(Qn::ActiveObjectPtzField); });
    }

    assert(!baseController->hasCapabilities(Qn::AsynchronousPtzCapability));
}

QnActivityPtzController::~QnActivityPtzController() {
    return;
}

bool QnActivityPtzController::extends(Qn::PtzCapabilities capabilities) {
    return 
        (capabilities & (Qn::PresetsPtzCapability | Qn::ToursPtzCapability)) &&
        !(capabilities & Qn::ActivityPtzCapability);
}

Qn::PtzCapabilities QnActivityPtzController::getCapabilities() {
    Qn::PtzCapabilities capabilities = base_type::getCapabilities();
    return extends(capabilities) ? (capabilities | Qn::ActivityPtzCapability) : capabilities;
}

bool QnActivityPtzController::continuousMove(const QVector3D &speed) {
    if(!base_type::continuousMove(speed))
        return false;

    setActiveObject(QnPtzObject());
    return true;
}

bool QnActivityPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(!base_type::absoluteMove(space, position, speed))
        return false;

    setActiveObject(QnPtzObject());
    return true;
}

bool QnActivityPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    if(!base_type::viewportMove(aspectRatio, viewport, speed))
        return false;

    setActiveObject(QnPtzObject());
    return true;
}

bool QnActivityPtzController::activatePreset(const QString &presetId, qreal speed) {
    if(!base_type::activatePreset(presetId, speed))
        return false;

    setActiveObject(QnPtzObject(Qn::PresetPtzObject, presetId));
    return true;
}

bool QnActivityPtzController::activateTour(const QString &tourId) {
    if(!base_type::activateTour(tourId))
        return false;

    setActiveObject(QnPtzObject(Qn::TourPtzObject, tourId));
    return true;
}

bool QnActivityPtzController::getActiveObject(QnPtzObject *activeObject) {
    if(m_isLocal) {
        *activeObject = m_activeObject;
    } else {
        *activeObject = m_adaptor->value();
    }
    return true;
}

void QnActivityPtzController::setActiveObject(const QnPtzObject &activeObject) {
    if(m_isLocal) {
        if(m_activeObject != activeObject) {
            m_activeObject = activeObject;
            emit changed(Qn::ActiveObjectPtzField);
        }
    } else {
        m_adaptor->setValue(activeObject);
    }
}
