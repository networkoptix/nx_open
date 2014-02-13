#include "activity_ptz_controller.h"

#include <api/resource_property_adaptor.h>

QnActivityPtzController::QnActivityPtzController(Mode mode, const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_mode(mode),
    m_adaptor(NULL)
{
    if(m_mode != Local) {
        m_adaptor = new QnJsonResourcePropertyAdaptor<QnPtzObject>(baseController->resource(), lit("ptzActiveObject"), QnPtzObject(), this);
        m_adaptor->setValue(QnPtzObject());
        connect(m_adaptor, &QnAbstractResourcePropertyAdaptor::valueChanged, this, [this]{ emit changed(Qn::ActiveObjectPtzField); });
    }
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

    if(m_mode != Client)
        setActiveObject(QnPtzObject());
    return true;
}

bool QnActivityPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(!base_type::absoluteMove(space, position, speed))
        return false;

    if(m_mode != Client)
        setActiveObject(QnPtzObject());
    return true;
}

bool QnActivityPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    if(!base_type::viewportMove(aspectRatio, viewport, speed))
        return false;

    if(m_mode != Client)
        setActiveObject(QnPtzObject());
    return true;
}

bool QnActivityPtzController::activatePreset(const QString &presetId, qreal speed) {
    if(!base_type::activatePreset(presetId, speed))
        return false;

    if(m_mode != Client)
        setActiveObject(QnPtzObject(Qn::PresetPtzObject, presetId));
    return true;
}

bool QnActivityPtzController::activateTour(const QString &tourId) {
    if(!base_type::activateTour(tourId))
        return false;

    if(m_mode != Client)
        setActiveObject(QnPtzObject(Qn::TourPtzObject, tourId));
    return true;
}

bool QnActivityPtzController::getActiveObject(QnPtzObject *activeObject) {
    if(m_mode == Local) {
        *activeObject = m_activeObject;
    } else {
        *activeObject = m_adaptor->value();
    }

    // TODO: #Elric #PTZ emit if asynchronous

    return true;
}

void QnActivityPtzController::setActiveObject(const QnPtzObject &activeObject) {
    if(m_mode == Local) {
        if(m_activeObject != activeObject) {
            m_activeObject = activeObject;
            emit changed(Qn::ActiveObjectPtzField);
        }
    } else {
        m_adaptor->setValue(activeObject);
    }
}
