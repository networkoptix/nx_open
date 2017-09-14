#include "caching_ptz_controller.h"

#include <common/common_meta_types.h>

#include <core/resource/resource.h>

#include <nx/utils/algorithm/index_of.h>

QnCachingPtzController::QnCachingPtzController(const QnPtzControllerPtr& baseController):
    base_type(baseController),
    m_initialized(false)
{
    if (initialize())
        return;

    /* Well, this is hacky. Sync can fail because we're behind a remote
     * PTZ controller for an offline camera. But strictly speaking,
     * we don't know that. Should probably be fixed by adding a signal to
     * PTZ controller. */ // TODO: #Elric

    QPointer<QnCachingPtzController> guard(this);

    /**
     * Prevents calling "initialize" function on removed "this". Controller could be removed
     * from QnPtzControllerPool after signals of resource emitted but not delivered.
     */
    const auto safeInitialize =
        [guard]()
        {
            if (guard)
                guard->initialize();
        };

    connect(resource(), &QnResource::statusChanged, this, safeInitialize);
    connect(resource(), &QnResource::parentIdChanged, this, safeInitialize);
}

QnCachingPtzController::~QnCachingPtzController()
{
}

bool QnCachingPtzController::extends(Ptz::Capabilities capabilities)
{
    return capabilities.testFlag(Ptz::AsynchronousPtzCapability)
        && !capabilities.testFlag(Ptz::SynchronizedPtzCapability);
}

Ptz::Capabilities QnCachingPtzController::getCapabilities() const
{
    const Ptz::Capabilities capabilities = base_type::getCapabilities();
    return extends(capabilities)
        ? capabilities | Ptz::SynchronizedPtzCapability
        : capabilities;
}

bool QnCachingPtzController::getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits* limits) const
{
    if (!base_type::getLimits(space, limits))
        return false;

    const QnMutexLocker locker(&m_mutex);
    switch (space)
    {
        case Qn::DevicePtzCoordinateSpace:
            if (!m_data.fields.testFlag(Qn::DeviceLimitsPtzField))
                return false;

            *limits = m_data.deviceLimits;
            return true;

        case Qn::LogicalPtzCoordinateSpace:
            if (!m_data.fields.testFlag(Qn::LogicalLimitsPtzField))
                return false;

            *limits = m_data.logicalLimits;
            return true;

        default:
            NX_EXPECT(false, "Wrong coordinate space");
            return false;
    }

    return false;
}

bool QnCachingPtzController::getFlip(Qt::Orientations* flip) const
{
    if (!base_type::getFlip(flip))
        return false;

    const QnMutexLocker locker(&m_mutex);
    if (!m_data.fields.testFlag(Qn::FlipPtzField))
        return false;

    *flip = m_data.flip;
    return true;
}

bool QnCachingPtzController::getPresets(QnPtzPresetList* presets) const
{
    if (!base_type::getPresets(presets))
        return false;

    const QnMutexLocker locker(&m_mutex);
    if (!m_data.fields.testFlag(Qn::PresetsPtzField))
        return false;

    *presets = m_data.presets;
    return true;
}

bool QnCachingPtzController::getTours(QnPtzTourList* tours) const
{
    if (!base_type::getTours(tours))
        return false;

    const QnMutexLocker locker(&m_mutex);
    if (!m_data.fields.testFlag(Qn::ToursPtzField))
        return false;

    *tours = m_data.tours;
    return true;
}

bool QnCachingPtzController::getActiveObject(QnPtzObject* activeObject) const
{
    if (!base_type::getActiveObject(activeObject))
        return false;

    const QnMutexLocker locker(&m_mutex);
    if (!m_data.fields.testFlag(Qn::ActiveObjectPtzField))
        return false;

    *activeObject = m_data.activeObject;
    return true;
}

bool QnCachingPtzController::getHomeObject(QnPtzObject* homeObject) const
{
    if (!base_type::getHomeObject(homeObject))
        return false;

    const QnMutexLocker locker(&m_mutex);
    if (!m_data.fields.testFlag(Qn::HomeObjectPtzField))
        return false;

    *homeObject = m_data.homeObject;
    return true;
}

bool QnCachingPtzController::getAuxilaryTraits(QnPtzAuxilaryTraitList* auxilaryTraits) const
{
    if (!base_type::getAuxilaryTraits(auxilaryTraits))
        return false;

    const QnMutexLocker locker(&m_mutex);
    if (!m_data.fields.testFlag(Qn::AuxilaryTraitsPtzField))
        return false;

    *auxilaryTraits = m_data.auxilaryTraits;
    return true;
}

bool QnCachingPtzController::getData(Qn::PtzDataFields query, QnPtzData* data) const
{
    // TODO: #Elric should be base_type::getData => bad design =(
    if (!baseController()->getData(query, data))
        return false;

    const QnMutexLocker locker(&m_mutex);
    *data = m_data;
    data->query = query;
    data->fields &= query;
    return true;
}

void QnCachingPtzController::baseFinished(Qn::PtzCommand command, const QVariant &data) {
    Qn::PtzDataFields changedFields = Qn::NoPtzFields;

    if (data.isValid()) {
        const QnMutexLocker locker(&m_mutex);
        switch (command) {
        case Qn::CreatePresetPtzCommand:
            if (m_data.fields & Qn::PresetsPtzField) {
                QnPtzPreset preset = data.value<QnPtzPreset>();
                int idx = nx::utils::algorithm::index_of(m_data.presets,
                    [&](const QnPtzPreset &old) { return old.id == preset.id; });
                if (idx < 0) {
                    m_data.presets.append(preset);
                    changedFields |= Qn::PresetsPtzField;
                } else if (m_data.presets[idx] != preset) {
                    m_data.presets[idx] = preset;
                    changedFields |= Qn::PresetsPtzField;
                }
            }
            break;
        case Qn::UpdatePresetPtzCommand:
            if (m_data.fields & Qn::PresetsPtzField) {
                QnPtzPreset preset = data.value<QnPtzPreset>();
                int idx = nx::utils::algorithm::index_of(m_data.presets,
                    [&](const QnPtzPreset &old) { return old.id == preset.id; });
                if (idx >= 0 && m_data.presets[idx] != preset) {
                    m_data.presets[idx] = preset;
                    changedFields |= Qn::PresetsPtzField;
                }
            }
            break;
        case Qn::RemovePresetPtzCommand:
            if (m_data.fields & Qn::PresetsPtzField) {
                QString presetId = data.value<QString>();
                int idx = nx::utils::algorithm::index_of(m_data.presets,
                    [&](const QnPtzPreset &old) { return old.id == presetId; });
                if (idx >= 0) {
                    m_data.presets.removeAt(idx);
                    changedFields |= Qn::PresetsPtzField;
                }
            }
            break;
        case Qn::CreateTourPtzCommand:
            if (m_data.fields & Qn::ToursPtzField) {
                QnPtzTour tour = data.value<QnPtzTour>();
                int idx = nx::utils::algorithm::index_of(m_data.tours,
                    [&](const QnPtzTour &old) { return old.id == tour.id; });
                if (idx < 0) {
                    m_data.tours.append(tour);
                    changedFields |= Qn::ToursPtzField;
                } else if (m_data.tours[idx] != tour) {
                    m_data.tours[idx] = tour;
                    changedFields |= Qn::ToursPtzField;
                }
            }
            break;
        case Qn::RemoveTourPtzCommand:
            if (m_data.fields & Qn::PresetsPtzField) {
                QString tourId = data.value<QString>();
                int idx = nx::utils::algorithm::index_of(m_data.tours,
                    [&](const QnPtzTour &old) { return old.id == tourId; });
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

    if (changedFields != Qn::NoPtzFields)
        emit changed(changedFields);
}

bool QnCachingPtzController::initialize()
{
    /* Note that this field is accessed from this object's thread only,
     * so there is no need to lock. */
    if (m_initialized)
        return true;

    if (resource()->hasFlags(Qn::foreigner))
        return false;

    QnPtzData data;
    return getData(Qn::AllPtzFields, &data);
}

template<class T>
Qn::PtzDataFields QnCachingPtzController::updateCacheLocked(Qn::PtzDataField field, T QnPtzData::*member, const T &value) {
    if ((m_data.fields & field) != field || m_data.*member != value) {
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
    if (data.query == Qn::AllPtzFields)
        m_initialized = true;

    /* We don't cache position as it doesn't make much sense. */
    Qn::PtzDataFields fields = data.fields & ~(Qn::DevicePositionPtzField | Qn::LogicalPositionPtzField);
    if (fields == Qn::NoPtzFields)
        return Qn::NoPtzFields;
    Qn::PtzDataFields changedFields = Qn::NoPtzFields;

    if (fields & Qn::DeviceLimitsPtzField)   changedFields |= updateCacheLocked(Qn::DeviceLimitsPtzField,    &QnPtzData::deviceLimits,   data.deviceLimits);
    if (fields & Qn::LogicalLimitsPtzField)  changedFields |= updateCacheLocked(Qn::LogicalLimitsPtzField,   &QnPtzData::logicalLimits,  data.logicalLimits);
    if (fields & Qn::FlipPtzField)           changedFields |= updateCacheLocked(Qn::FlipPtzField,            &QnPtzData::flip,           data.flip);
    if (fields & Qn::PresetsPtzField)        changedFields |= updateCacheLocked(Qn::PresetsPtzField,         &QnPtzData::presets,        data.presets);
    if (fields & Qn::ToursPtzField)          changedFields |= updateCacheLocked(Qn::ToursPtzField,           &QnPtzData::tours,          data.tours);
    if (fields & Qn::ActiveObjectPtzField)   changedFields |= updateCacheLocked(Qn::ActiveObjectPtzField,    &QnPtzData::activeObject,   data.activeObject);
    if (fields & Qn::HomeObjectPtzField)     changedFields |= updateCacheLocked(Qn::HomeObjectPtzField,      &QnPtzData::homeObject,     data.homeObject);
    if (fields & Qn::AuxilaryTraitsPtzField) changedFields |= updateCacheLocked(Qn::AuxilaryTraitsPtzField,  &QnPtzData::auxilaryTraits, data.auxilaryTraits);

    return changedFields;
}
