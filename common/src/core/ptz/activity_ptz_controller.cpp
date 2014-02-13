#include "activity_ptz_controller.h"

#include <cassert>

#include <api/resource_property_adaptor.h>

QnActivityPtzController::QnActivityPtzController(bool isLocal, const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_isLocal(isLocal)
{
    if(m_isLocal) {
        m_adaptor = new QnJsonResourcePropertyAdaptor<QnPtzObject>(baseController->resource(), lit("activePtzObject"), QnPtzObject(), this);
        connect(m_adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged, this, &QnActivityPtzController::at_adaptor_valueChanged);
    }

    assert(!baseController->hasCapabilities(Qn::AsynchronousPtzCapability));
}

QnActivityPtzController::~QnActivityPtzController() {
    return;
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
            emit capabilitiesChanged(); // TODO: #Elric
        }
    } else {
        m_adaptor->setValue(activeObject);
    }
}

void QnActivityPtzController::at_adaptor_valueChanged() {
    emit capabilitiesChanged(); // TODO: #Elric
}
