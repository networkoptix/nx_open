#include "activity_ptz_controller.h"

#include <api/resource_property_adaptor.h>

QnActivityPtzController::QnActivityPtzController(Mode mode, const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_mode(mode),
    m_adaptor(NULL)
{
    m_asynchronous = baseController->hasCapabilities(Qn::AsynchronousPtzCapability);

    if(m_mode != Local) {
        m_adaptor = new QnJsonResourcePropertyAdaptor<QnPtzObject>(baseController->resource(), lit("ptzActiveObject"), QnPtzObject(), this);
        m_adaptor->setValue(QnPtzObject());
        connect(m_adaptor, &QnAbstractResourcePropertyAdaptor::valueChangedExternally, this, [this]{ emit changed(Qn::ActiveObjectPtzField); });
    }

    // TODO: #Elric #PTZ better async support
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

bool QnActivityPtzController::removePreset(const QString &presetId) {
    if(!base_type::removePreset(presetId))
        return false;

    bool activeObjectChanged = false;
    {
        QMutexLocker locker(&m_mutex);
        if(getActiveObjectLocked() == QnPtzObject(Qn::PresetPtzObject, presetId))
            activeObjectChanged = setActiveObjectLocked(QnPtzObject());
    }
    
    if(activeObjectChanged)
        emit changed(Qn::ActiveObjectPtzField);

    return true;
}

bool QnActivityPtzController::activatePreset(const QString &presetId, qreal speed) {
    if(!base_type::activatePreset(presetId, speed))
        return false;

    if(m_mode != Client)
        setActiveObject(QnPtzObject(Qn::PresetPtzObject, presetId));

    return true;
}

bool QnActivityPtzController::removeTour(const QString &tourId) {
    if(!base_type::removeTour(tourId))
        return false;

    bool activeObjectChanged = false;
    {
        QMutexLocker locker(&m_mutex);
        if(getActiveObjectLocked() == QnPtzObject(Qn::TourPtzObject, tourId))
            activeObjectChanged = setActiveObjectLocked(QnPtzObject());
    }

    if(activeObjectChanged)
        emit changed(Qn::ActiveObjectPtzField);

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
    *activeObject = getActiveObject();
    
    return true;
}

bool QnActivityPtzController::getData(Qn::PtzDataFields query, QnPtzData *data) {
    // TODO: #Elric #PTZ this is a hack. Need to do it better.
    if(m_asynchronous) {
        return baseController()->getData(query, data);
    } else {
        return base_type::getData(query, data);
    }
}

QnPtzObject QnActivityPtzController::getActiveObject() {
    QMutexLocker locker(&m_mutex);
    return getActiveObjectLocked();
}

void QnActivityPtzController::setActiveObject(const QnPtzObject &activeObject) {
    bool activeObjectChanged = false;
    {
        QMutexLocker locker(&m_mutex);
        activeObjectChanged = setActiveObjectLocked(activeObject);
    }
    if(activeObjectChanged)
        emit changed(Qn::ActiveObjectPtzField);
}

QnPtzObject QnActivityPtzController::getActiveObjectLocked() const {
    if(m_mode == Local) {
        return m_activeObject;
    } else {
        return m_adaptor->value();
    }
}

bool QnActivityPtzController::setActiveObjectLocked(const QnPtzObject &activeObject) {
    if(m_mode == Local) {
        if(m_activeObject != activeObject) {
            m_activeObject = activeObject;
            return true;
        }
    } else {
        if(m_adaptor->value() != activeObject) {
            m_adaptor->setValue(activeObject);
            return true;
        }
    }

    return false;
}
