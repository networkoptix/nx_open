#include "caching_ptz_controller.h"

#include <common/common_meta_types.h>

#include <core/resource/resource.h>

#include <utils/common/container.h>

QnCachingPtzController::QnCachingPtzController(const QnPtzControllerPtr &baseController):
    base_type(baseController),
    m_initialized(false)
{
    if(!initialize()) {
        /* Well, this is hacky. Sync can fail because we're behind a remote
         * PTZ controller for an offline camera. But strictly speaking, 
         * we don't know that. Should probably be fixed by adding a signal to
         * PTZ controller. */ // TODO: #Elric
        connect(resource(), &QnResource::statusChanged, this, &QnCachingPtzController::initialize);
    }
}

QnCachingPtzController::~QnCachingPtzController() {
    return;
}

bool QnCachingPtzController::extends(Qn::PtzCapabilities capabilities) {
    return 
        (capabilities & Qn::AsynchronousPtzCapability) &&
        !(capabilities & Qn::SynchronizedPtzCapability);
}

Qn::PtzCapabilities QnCachingPtzController::getCapabilities() {
    Qn::PtzCapabilities capabilities = base_type::getCapabilities();
    return extends(capabilities) ? (capabilities | Qn::SynchronizedPtzCapability) : capabilities;
}

bool QnCachingPtzController::continuousMove(const QVector3D &speed) {
    if(!base_type::continuousMove(speed))
        return false;

    QMutexLocker locker(&m_mutex);
    m_data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);
    return true;
}

bool QnCachingPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    if(!base_type::absoluteMove(space, position, speed))
        return false;

    QMutexLocker locker(&m_mutex);
    m_data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);
    return true;
}

bool QnCachingPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    if(!base_type::viewportMove(aspectRatio, viewport, speed))
        return false;

    QMutexLocker locker(&m_mutex);
    m_data.fields &= ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);
    return true;
}

bool QnCachingPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    if(!base_type::getPosition(space, position))
        return false;

    QMutexLocker locker(&m_mutex);
    if(space == Qn::DevicePtzCoordinateSpace) {
        if(m_data.fields & Qn::DevicePositionPtzField) {
            *position = m_data.devicePosition;
            return true;
        } else {
            return false;
        }
    } else {
        if(m_data.fields & Qn::LogicalPositionPtzField) {
            *position = m_data.logicalPosition;
            return true;
        } else {
            return false;
        }
    }
}

bool QnCachingPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) {
    if(!base_type::getLimits(space, limits))
        return false;

    QMutexLocker locker(&m_mutex);
    if(space == Qn::DevicePtzCoordinateSpace) {
        if(m_data.fields & Qn::DeviceLimitsPtzField) {
            *limits = m_data.deviceLimits;
            return true;
        } else {
            return false;
        }
    } else {
        if(m_data.fields & Qn::LogicalLimitsPtzField) {
            *limits = m_data.logicalLimits;
            return true;
        } else {
            return false;
        }
    }
}

bool QnCachingPtzController::getFlip(Qt::Orientations *flip) {
    if(!base_type::getFlip(flip))
        return false;

    QMutexLocker locker(&m_mutex);
    if(m_data.fields & Qn::FlipPtzField) {
        *flip = m_data.flip;
        return true;
    } else {
        return false;
    }
}

bool QnCachingPtzController::createPreset(const QnPtzPreset &preset) {
    return base_type::createPreset(preset); 
}

bool QnCachingPtzController::updatePreset(const QnPtzPreset &preset) {
    return base_type::updatePreset(preset);
}

bool QnCachingPtzController::removePreset(const QString &presetId) {
    return base_type::removePreset(presetId);
}

bool QnCachingPtzController::activatePreset(const QString &presetId, qreal speed) {
    return base_type::activatePreset(presetId, speed);
}

bool QnCachingPtzController::getPresets(QnPtzPresetList *presets) {
    if(!base_type::getPresets(presets))
        return false;

    QMutexLocker locker(&m_mutex);
    if(m_data.fields & Qn::PresetsPtzField) {
        *presets = m_data.presets;
        return true;
    } else {
        return false;
    }
}

bool QnCachingPtzController::createTour(const QnPtzTour &tour) {
    return base_type::createTour(tour); 
}

bool QnCachingPtzController::removeTour(const QString &tourId) {
    return base_type::removeTour(tourId); 
}

bool QnCachingPtzController::activateTour(const QString &tourId) {
    return base_type::activateTour(tourId); 
}

bool QnCachingPtzController::getTours(QnPtzTourList *tours) {
    if(!base_type::getTours(tours))
        return false;

    QMutexLocker locker(&m_mutex);
    if(m_data.fields & Qn::ToursPtzField) {
        *tours = m_data.tours;
        return true;
    } else {
        return false;
    }
}

bool QnCachingPtzController::getActiveObject(QnPtzObject *activeObject) {
    if(!base_type::getActiveObject(activeObject))
        return false;

    QMutexLocker locker(&m_mutex);
    if(m_data.fields & Qn::ActiveObjectPtzField) {
        *activeObject = m_data.activeObject;
        return true;
    } else {
        return false;
    }
}

bool QnCachingPtzController::updateHomeObject(const QnPtzObject &homeObject) {
    return base_type::updateHomeObject(homeObject); 
}

bool QnCachingPtzController::getHomeObject(QnPtzObject *homeObject) {
    if(!base_type::getHomeObject(homeObject))
        return false;

    QMutexLocker locker(&m_mutex);
    if(m_data.fields & Qn::HomeObjectPtzField) {
        *homeObject = m_data.homeObject;
        return true;
    } else {
        return false;
    }
}

bool QnCachingPtzController::getData(Qn::PtzDataFields query, QnPtzData *data) {
    if(!baseController()->getData(query, data)) // TODO: #Elric should be base_type::getData => bad design =(
        return false;

    QMutexLocker locker(&m_mutex);
    *data = m_data;
    data->query = query;
    data->fields &= query;
    return true;
}

void QnCachingPtzController::baseFinished(Qn::PtzCommand command, const QVariant &data) {
    if(data.isValid()) {
        QMutexLocker locker(&m_mutex);
        switch (command) {
        case Qn::CreatePresetPtzCommand:
            if(m_data.fields & Qn::PresetsPtzField) {
                QnPtzPreset preset = data.value<QnPtzPreset>();
                int idx = qnIndexOf(m_data.presets, [&](const QnPtzPreset &old) { return old.id == preset.id; });
                if (idx < 0)
                    m_data.presets.append(preset);
                else
                    m_data.presets[idx] = preset;
            }
            break;
        case Qn::UpdatePresetPtzCommand:
            if(m_data.fields & Qn::PresetsPtzField) {
                QnPtzPreset preset = data.value<QnPtzPreset>();
                int idx = qnIndexOf(m_data.presets, [&](const QnPtzPreset &old) { return old.id == preset.id; });
                if (idx >= 0)
                    m_data.presets[idx] = preset;
            }
            break;
        case Qn::RemovePresetPtzCommand:
            if(m_data.fields & Qn::PresetsPtzField) {
                QString presetId = data.value<QString>();
                int idx = qnIndexOf(m_data.presets, [&](const QnPtzPreset &old) { return old.id == presetId; });
                if (idx >= 0)
                    m_data.presets.removeAt(idx);
            }
            break;
        case Qn::CreateTourPtzCommand:
            if(m_data.fields & Qn::ToursPtzField) {
                QnPtzTour tour = data.value<QnPtzTour>();
                int idx = qnIndexOf(m_data.tours, [&](const QnPtzTour &old) { return old.id == tour.id; });
                if (idx < 0)
                    m_data.tours.append(tour);
                else
                    m_data.tours[idx] = tour;
            }
            break;
        case Qn::RemoveTourPtzCommand:
            if(m_data.fields & Qn::PresetsPtzField) {
                QString tourId = data.value<QString>();
                int idx = qnIndexOf(m_data.tours, [&](const QnPtzTour &old) { return old.id == tourId; });
                if (idx >= 0)
                    m_data.tours.removeAt(idx);
            }
            break;
        case Qn::GetDeviceLimitsPtzCommand:
            m_data.fields |= Qn::DeviceLimitsPtzField;
            m_data.deviceLimits = data.value<QnPtzLimits>();
            break;
        case Qn::GetLogicalLimitsPtzCommand:
            m_data.fields |= Qn::LogicalLimitsPtzField;
            m_data.logicalLimits = data.value<QnPtzLimits>();
            break;
        case Qn::GetFlipPtzCommand:
            m_data.fields |= Qn::FlipPtzField;
            m_data.flip = data.value<Qt::Orientations>();
            break;
        case Qn::GetPresetsPtzCommand:
            m_data.fields |= Qn::PresetsPtzField;
            m_data.presets = data.value<QnPtzPresetList>();
            break;
        case Qn::GetToursPtzCommand:
            m_data.fields |= Qn::ToursPtzField;
            m_data.tours = data.value<QnPtzTourList>();
            break;
        case Qn::GetActiveObjectPtzCommand:
            m_data.fields |= Qn::ActiveObjectPtzField;
            m_data.activeObject = data.value<QnPtzObject>();
            break;
        case Qn::UpdateHomeObjectPtzCommand:
        case Qn::GetHomeObjectPtzCommand:
            m_data.fields |= Qn::HomeObjectPtzField;
            m_data.homeObject = data.value<QnPtzObject>();
            break;
        case Qn::GetDataPtzCommand:
            updateCacheLocked(data.value<QnPtzData>());
            break;
        default:
            break;
        }
    }

    base_type::baseFinished(command, data);
}

bool QnCachingPtzController::initialize() {
    /* Note that this field is accessed from this object's thread only,
     * so there is no need to lock. */
    if(m_initialized)
        return true;

    QnPtzData data;
    return getData(Qn::AllPtzFields, &data);
}

void QnCachingPtzController::updateCacheLocked(const QnPtzData &data) {
    if(data.query == Qn::AllPtzFields)
        m_initialized = true;

    Qn::PtzDataFields fields = data.fields & ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);
    if(fields == Qn::NoPtzFields)
        return;

    if(fields & Qn::DeviceLimitsPtzField)   m_data.deviceLimits = data.deviceLimits;
    if(fields & Qn::LogicalLimitsPtzField)  m_data.logicalLimits = data.logicalLimits;
    if(fields & Qn::FlipPtzField)           m_data.flip = data.flip;
    if(fields & Qn::PresetsPtzField)        m_data.presets = data.presets;
    if(fields & Qn::ToursPtzField)          m_data.tours = data.tours;
    if(fields & Qn::ActiveObjectPtzField)   m_data.activeObject = data.activeObject;
    if(fields & Qn::HomeObjectPtzField)     m_data.homeObject = data.homeObject;
    m_data.fields |= fields;
}
