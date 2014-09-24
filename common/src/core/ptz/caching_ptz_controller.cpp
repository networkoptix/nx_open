#include "caching_ptz_controller.h"

#include <common/common_meta_types.h>

#include <core/resource/resource.h>

#include <utils/common/collection.h>

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
    return base_type::continuousMove(speed);
}

bool QnCachingPtzController::continuousFocus(qreal speed) {
    return base_type::continuousFocus(speed);
}

bool QnCachingPtzController::absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) {
    return base_type::absoluteMove(space, position, speed);
}

bool QnCachingPtzController::viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) {
    return base_type::viewportMove(aspectRatio, viewport, speed);
}

bool QnCachingPtzController::getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) {
    /* We don't cache position => no need to check cache here. */
    return base_type::getPosition(space, position);
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

bool QnCachingPtzController::getAuxilaryTraits(QnPtzAuxilaryTraitList *auxilaryTraits) {
    if(!base_type::getAuxilaryTraits(auxilaryTraits))
        return false;

    QMutexLocker locker(&m_mutex);
    if(m_data.fields & Qn::AuxilaryTraitsPtzField) {
        *auxilaryTraits = m_data.auxilaryTraits;
        return true;
    } else {
        return false;
    }
}

bool QnCachingPtzController::runAuxilaryCommand(const QnPtzAuxilaryTrait &trait, const QString &data) {
    return base_type::runAuxilaryCommand(trait, data); 
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
    Qn::PtzDataFields changedFields = Qn::NoPtzFields;

    if(data.isValid()) {
        QMutexLocker locker(&m_mutex);
        switch (command) {
        case Qn::CreatePresetPtzCommand:
            if(m_data.fields & Qn::PresetsPtzField) {
                QnPtzPreset preset = data.value<QnPtzPreset>();
                int idx = qnIndexOf(m_data.presets, [&](const QnPtzPreset &old) { return old.id == preset.id; });
                if (idx < 0) {
                    m_data.presets.append(preset);
                    changedFields |= Qn::PresetsPtzField;
                } else if(m_data.presets[idx] != preset) {
                    m_data.presets[idx] = preset;
                    changedFields |= Qn::PresetsPtzField;
                }
            }
            break;
        case Qn::UpdatePresetPtzCommand:
            if(m_data.fields & Qn::PresetsPtzField) {
                QnPtzPreset preset = data.value<QnPtzPreset>();
                int idx = qnIndexOf(m_data.presets, [&](const QnPtzPreset &old) { return old.id == preset.id; });
                if (idx >= 0 && m_data.presets[idx] != preset) {
                    m_data.presets[idx] = preset;
                    changedFields |= Qn::PresetsPtzField;
                }
            }
            break;
        case Qn::RemovePresetPtzCommand:
            if(m_data.fields & Qn::PresetsPtzField) {
                QString presetId = data.value<QString>();
                int idx = qnIndexOf(m_data.presets, [&](const QnPtzPreset &old) { return old.id == presetId; });
                if (idx >= 0) {
                    m_data.presets.removeAt(idx);
                    changedFields |= Qn::PresetsPtzField;
                }
            }
            break;
        case Qn::CreateTourPtzCommand:
            if(m_data.fields & Qn::ToursPtzField) {
                QnPtzTour tour = data.value<QnPtzTour>();
                int idx = qnIndexOf(m_data.tours, [&](const QnPtzTour &old) { return old.id == tour.id; });
                if (idx < 0) {
                    m_data.tours.append(tour);
                    changedFields |= Qn::ToursPtzField;
                } else if(m_data.tours[idx] != tour) {
                    m_data.tours[idx] = tour;
                    changedFields |= Qn::ToursPtzField;
                }
            }
            break;
        case Qn::RemoveTourPtzCommand:
            if(m_data.fields & Qn::PresetsPtzField) {
                QString tourId = data.value<QString>();
                int idx = qnIndexOf(m_data.tours, [&](const QnPtzTour &old) { return old.id == tourId; });
                if (idx >= 0) {
                    m_data.tours.removeAt(idx);
                    changedFields |= Qn::ToursPtzField;
                }
            }
            break;
        case Qn::GetDeviceLimitsPtzCommand:
            changedFields |= updateCacheLocked(Qn::DeviceLimitsPtzField, &QnPtzData::deviceLimits, data);
            break;
        case Qn::GetLogicalLimitsPtzCommand:
            changedFields |= updateCacheLocked(Qn::LogicalLimitsPtzField, &QnPtzData::logicalLimits, data);
            break;
        case Qn::GetFlipPtzCommand:
            changedFields |= updateCacheLocked(Qn::FlipPtzField, &QnPtzData::flip, data);
            break;
        case Qn::GetPresetsPtzCommand:
            changedFields |= updateCacheLocked(Qn::PresetsPtzField, &QnPtzData::presets, data);
            break;
        case Qn::GetToursPtzCommand:
            changedFields |= updateCacheLocked(Qn::ToursPtzField, &QnPtzData::tours, data);
            break;
        case Qn::GetActiveObjectPtzCommand:
            changedFields |= updateCacheLocked(Qn::ActiveObjectPtzField, &QnPtzData::activeObject, data);
            break;
        case Qn::UpdateHomeObjectPtzCommand:
        case Qn::GetHomeObjectPtzCommand:
            changedFields |= updateCacheLocked(Qn::HomeObjectPtzField, &QnPtzData::homeObject, data);
            break;
        case Qn::GetAuxilaryTraitsPtzCommand:
            changedFields |= updateCacheLocked(Qn::AuxilaryTraitsPtzField, &QnPtzData::auxilaryTraits, data);
            break;
        case Qn::GetDataPtzCommand:
            changedFields |= updateCacheLocked(data.value<QnPtzData>());
            break;
        default:
            break;
        }
    }

    base_type::baseFinished(command, data);

    if(changedFields != Qn::NoPtzFields)
        emit changed(changedFields);
}

bool QnCachingPtzController::initialize() {
    /* Note that this field is accessed from this object's thread only,
     * so there is no need to lock. */
    if(m_initialized)
        return true;

    QnPtzData data;
    return getData(Qn::AllPtzFields, &data);
}

template<class T>
Qn::PtzDataFields QnCachingPtzController::updateCacheLocked(Qn::PtzDataField field, T QnPtzData::*member, const T &value) {
    if((m_data.fields & field) != field || m_data.*member != value) {
        m_data.fields |= field;
        m_data.*member = value;
        return field;
    } else {
        return Qn::NoPtzFields;
    }
}

template<class T>
Qn::PtzDataFields QnCachingPtzController::updateCacheLocked(Qn::PtzDataField field, T QnPtzData::*member, const QVariant &value) {
    return updateCacheLocked(field, member, value.value<T>());
}

Qn::PtzDataFields QnCachingPtzController::updateCacheLocked(const QnPtzData &data) {
    if(data.query == Qn::AllPtzFields)
        m_initialized = true;

    /* We don't cache position as it doesn't make much sense. */
    Qn::PtzDataFields fields = data.fields & ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);
    if(fields == Qn::NoPtzFields)
        return Qn::NoPtzFields;
    Qn::PtzDataFields changedFields = Qn::NoPtzFields;

    if(fields & Qn::DeviceLimitsPtzField)   changedFields |= updateCacheLocked(Qn::DeviceLimitsPtzField,    &QnPtzData::deviceLimits,   data.deviceLimits);
    if(fields & Qn::LogicalLimitsPtzField)  changedFields |= updateCacheLocked(Qn::LogicalLimitsPtzField,   &QnPtzData::logicalLimits,  data.logicalLimits);
    if(fields & Qn::FlipPtzField)           changedFields |= updateCacheLocked(Qn::FlipPtzField,            &QnPtzData::flip,           data.flip);
    if(fields & Qn::PresetsPtzField)        changedFields |= updateCacheLocked(Qn::PresetsPtzField,         &QnPtzData::presets,        data.presets);
    if(fields & Qn::ToursPtzField)          changedFields |= updateCacheLocked(Qn::ToursPtzField,           &QnPtzData::tours,          data.tours);
    if(fields & Qn::ActiveObjectPtzField)   changedFields |= updateCacheLocked(Qn::ActiveObjectPtzField,    &QnPtzData::activeObject,   data.activeObject);
    if(fields & Qn::HomeObjectPtzField)     changedFields |= updateCacheLocked(Qn::HomeObjectPtzField,      &QnPtzData::homeObject,     data.homeObject);
    if(fields & Qn::AuxilaryTraitsPtzField) changedFields |= updateCacheLocked(Qn::AuxilaryTraitsPtzField,  &QnPtzData::auxilaryTraits, data.auxilaryTraits);
    
    return changedFields;
}
