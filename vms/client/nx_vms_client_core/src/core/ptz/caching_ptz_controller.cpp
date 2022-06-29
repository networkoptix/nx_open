// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "caching_ptz_controller.h"

#include <common/common_meta_types.h>

#include <core/resource/resource.h>

#include <utils/common/delayed.h>

#include <nx/utils/algorithm/index_of.h>

using namespace nx::vms::common::ptz;

QnCachingPtzController::QnCachingPtzController(const QnPtzControllerPtr& baseController):
    base_type(baseController),
    m_initialized(false)
{
}

QnCachingPtzController::~QnCachingPtzController()
{
}

bool QnCachingPtzController::extends(Ptz::Capabilities capabilities)
{
    return capabilities.testFlag(Ptz::AsynchronousPtzCapability)
        && !capabilities.testFlag(Ptz::SynchronizedPtzCapability);
}

Ptz::Capabilities QnCachingPtzController::getCapabilities(
    const Options& options) const
{
    const Ptz::Capabilities capabilities = base_type::getCapabilities(options);
    return extends(capabilities)
        ? capabilities | Ptz::SynchronizedPtzCapability
        : capabilities;
}

bool QnCachingPtzController::getLimits(
    QnPtzLimits* limits,
    CoordinateSpace space,
    const Options& options) const
{
    if (!base_type::getLimits(limits, space, options))
        return false;

    const NX_MUTEX_LOCKER locker(&m_mutex);
    switch (space)
    {
        case CoordinateSpace::device:
            if (!m_data.fields.testFlag(DataField::deviceLimits))
                return false;

            *limits = m_data.deviceLimits;
            return true;

        case CoordinateSpace::logical:
            if (!m_data.fields.testFlag(DataField::logicalLimits))
                return false;

            *limits = m_data.logicalLimits;
            return true;

        default:
            NX_ASSERT(false, "Wrong coordinate space");
            return false;
    }

    return false;
}

bool QnCachingPtzController::getFlip(
    Qt::Orientations* flip,
    const Options& options) const
{
    if (!base_type::getFlip(flip, options))
        return false;

    const NX_MUTEX_LOCKER locker(&m_mutex);
    if (!m_data.fields.testFlag(DataField::flip))
        return false;

    *flip = m_data.flip;
    return true;
}

bool QnCachingPtzController::getPresets(QnPtzPresetList* presets) const
{
    if (!base_type::getPresets(presets))
        return false;

    const NX_MUTEX_LOCKER locker(&m_mutex);
    if (!m_data.fields.testFlag(DataField::presets))
        return false;

    *presets = m_data.presets;
    return true;
}

bool QnCachingPtzController::getTours(QnPtzTourList* tours) const
{
    if (!base_type::getTours(tours))
        return false;

    const NX_MUTEX_LOCKER locker(&m_mutex);
    if (!m_data.fields.testFlag(DataField::tours))
        return false;

    *tours = m_data.tours;
    return true;
}

bool QnCachingPtzController::getActiveObject(QnPtzObject* activeObject) const
{
    if (!base_type::getActiveObject(activeObject))
        return false;

    const NX_MUTEX_LOCKER locker(&m_mutex);
    if (!m_data.fields.testFlag(DataField::activeObject))
        return false;

    *activeObject = m_data.activeObject;
    return true;
}

bool QnCachingPtzController::getHomeObject(QnPtzObject* homeObject) const
{
    if (!base_type::getHomeObject(homeObject))
        return false;

    const NX_MUTEX_LOCKER locker(&m_mutex);
    if (!m_data.fields.testFlag(DataField::homeObject))
        return false;

    *homeObject = m_data.homeObject;
    return true;
}

bool QnCachingPtzController::getAuxiliaryTraits(
    QnPtzAuxiliaryTraitList* auxiliaryTraits,
    const Options& options) const
{
    if (!base_type::getAuxiliaryTraits(auxiliaryTraits, options))
        return false;

    const NX_MUTEX_LOCKER locker(&m_mutex);
    if (!m_data.fields.testFlag(DataField::auxiliaryTraits))
        return false;

    *auxiliaryTraits = m_data.auxiliaryTraits;
    return true;
}

bool QnCachingPtzController::getData(
    QnPtzData* data,
    DataFields query,
    const Options& options) const
{
    // TODO: #sivanov There is something really wrong with the thread safety in these classes.
    const auto base = baseController();
    // TODO: #sivanov Should be base_type::getData => bad design =(
    if (!base || !base->getData(data, query, options))
        return false;

    const NX_MUTEX_LOCKER locker(&m_mutex);
    *data = m_data;
    data->query = query;
    data->fields &= query;
    return true;
}

void QnCachingPtzController::baseFinished(Command command, const QVariant& data)
{
    DataFields changedFields = DataField::none;

    if (data.isValid())
    {
        const NX_MUTEX_LOCKER locker(&m_mutex);
        switch (command)
        {
            case Command::createPreset:
                if (m_data.fields.testFlag(DataField::presets))
                {
                    const auto preset = data.value<QnPtzPreset>();
                    int idx = nx::utils::algorithm::index_of(m_data.presets,
                        [&](const QnPtzPreset& old) { return old.id == preset.id; });
                    if (idx < 0)
                    {
                        m_data.presets.append(preset);
                        changedFields |= DataField::presets;
                    }
                    else if (m_data.presets[idx] != preset)
                    {
                        m_data.presets[idx] = preset;
                        changedFields |= DataField::presets;
                    }
                }
                break;
            case Command::updatePreset:
                if (m_data.fields.testFlag(DataField::presets))
                {
                    const auto preset = data.value<QnPtzPreset>();
                    int idx = nx::utils::algorithm::index_of(m_data.presets,
                        [&](const QnPtzPreset& old) { return old.id == preset.id; });
                    if (idx >= 0 && m_data.presets[idx] != preset)
                    {
                        m_data.presets[idx] = preset;
                        changedFields |= DataField::presets;
                    }
                }
                break;
            case Command::removePreset:
                if (m_data.fields.testFlag(DataField::presets))
                {
                    const auto presetId = data.value<QString>();
                    int idx = nx::utils::algorithm::index_of(m_data.presets,
                        [&](const QnPtzPreset& old) { return old.id == presetId; });
                    if (idx >= 0)
                    {
                        m_data.presets.removeAt(idx);
                        changedFields |= DataField::presets;
                    }
                }
                break;
            case Command::createTour:
                if (m_data.fields.testFlag(DataField::tours))
                {
                    const auto tour = data.value<QnPtzTour>();
                    int idx = nx::utils::algorithm::index_of(
                        m_data.tours, [&](const QnPtzTour& old) { return old.id == tour.id; });
                    if (idx < 0)
                    {
                        m_data.tours.append(tour);
                        changedFields |= DataField::tours;
                    }
                    else if (m_data.tours[idx] != tour)
                    {
                        m_data.tours[idx] = tour;
                        changedFields |= DataField::tours;
                    }
                }
                break;
            case Command::removeTour:
                if (m_data.fields.testFlag(DataField::tours))
                {
                    const auto tourId = data.value<QString>();
                    int idx = nx::utils::algorithm::index_of(
                        m_data.tours, [&](const QnPtzTour& old) { return old.id == tourId; });
                    if (idx >= 0)
                    {
                        m_data.tours.removeAt(idx);
                        changedFields |= DataField::tours;
                    }
                }
                break;
            case Command::getDeviceLimits:
                changedFields |=
                    updateCacheLocked(DataField::deviceLimits, &QnPtzData::deviceLimits, data);
                break;
            case Command::getLogicalLimits:
                changedFields |=
                    updateCacheLocked(DataField::logicalLimits, &QnPtzData::logicalLimits, data);
                break;
            case Command::getFlip:
                changedFields |= updateCacheLocked(DataField::flip, &QnPtzData::flip, data);
                break;
            case Command::getPresets:
                changedFields |= updateCacheLocked(DataField::presets, &QnPtzData::presets, data);
                break;
            case Command::getTours:
                changedFields |= updateCacheLocked(DataField::tours, &QnPtzData::tours, data);
                break;
            case Command::getActiveObject:
                changedFields |=
                    updateCacheLocked(DataField::activeObject, &QnPtzData::activeObject, data);
                break;
            case Command::updateHomeObject:
            case Command::getHomeObject:
                changedFields |=
                    updateCacheLocked(DataField::homeObject, &QnPtzData::homeObject, data);
                break;
            case Command::getAuxiliaryTraits:
                changedFields |= updateCacheLocked(
                    DataField::auxiliaryTraits, &QnPtzData::auxiliaryTraits, data);
                break;
            case Command::getData:
                changedFields |= updateCacheLocked(data.value<QnPtzData>());
                break;
            default:
                break;
        }
    }

    base_type::baseFinished(command, data);

    if (changedFields)
        emit changed(changedFields);
}

bool QnCachingPtzController::initializeInternal()
{
    /* Note that this field is accessed from this object's thread only,
     * so there is no need to lock. */
    if (m_initialized)
        return true;

    QnPtzData data;
    return getData(&data, DataField::all, {Type::operational});
}

void QnCachingPtzController::initialize()
{
    const auto guard = toSharedPointer();

    /**
     * Prevents calling "initialize" function on removed "this". Controller could be removed
     * from QnPtzControllerPool after signals of resource emitted but not delivered.
     */
    const auto safeInitialize =
        [weakRef = guard.toWeakRef()]()
        {
            if (auto controller = weakRef.lock())
                controller->initializeInternal();
        };

    connect(resource().get(), &QnResource::statusChanged, this, safeInitialize);
    connect(resource().get(), &QnResource::parentIdChanged, this, safeInitialize);
    initializeInternal();
}

template<class T>
DataFields QnCachingPtzController::updateCacheLocked(
    DataField field, T QnPtzData::*member, const T& value)
{
    if (!m_data.fields.testFlag(field) || m_data.*member != value)
    {
        m_data.fields |= field;
        m_data.*member = value;
        return field;
    }
    else
    {
        return DataField::none;
    }
}

template<class T>
DataFields QnCachingPtzController::updateCacheLocked(
    DataField field, T QnPtzData::*member, const QVariant& value)
{
    return updateCacheLocked(field, member, value.value<T>());
}

DataFields QnCachingPtzController::updateCacheLocked(const QnPtzData& data)
{
    using DataFields = nx::vms::common::ptz::DataFields;
    if (data.query == DataFields(DataField::all))
        m_initialized = true;

    /* We don't cache position as it doesn't make much sense. */
    DataFields fields =
        data.fields & ~(DataField::devicePosition | DataField::logicalPosition);

    if (fields == DataFields(DataField::none))
        return DataField::none;

    DataFields changedFields = DataField::none;

    if (fields.testFlag(DataField::deviceLimits))
    {
        changedFields |= updateCacheLocked(
            DataField::deviceLimits, &QnPtzData::deviceLimits, data.deviceLimits);
    }

    if (fields.testFlag(DataField::logicalLimits))
    {
        changedFields |= updateCacheLocked(
            DataField::logicalLimits, &QnPtzData::logicalLimits, data.logicalLimits);
    }

    if (fields.testFlag(DataField::flip))
    {
        changedFields |= updateCacheLocked(DataField::flip, &QnPtzData::flip, data.flip);
    }

    if (fields.testFlag(DataField::presets))
    {
        changedFields |= updateCacheLocked(DataField::presets, &QnPtzData::presets, data.presets);
    }

    if (fields.testFlag(DataField::tours))
    {
        changedFields |= updateCacheLocked(DataField::tours, &QnPtzData::tours, data.tours);
    }

    if (fields.testFlag(DataField::activeObject))
    {
        changedFields |= updateCacheLocked(
            DataField::activeObject, &QnPtzData::activeObject, data.activeObject);
    }

    if (fields.testFlag(DataField::homeObject))
    {
        changedFields |= updateCacheLocked(
            DataField::homeObject, &QnPtzData::homeObject, data.homeObject);
    }

    if (fields.testFlag(DataField::auxiliaryTraits))
    {
        changedFields |= updateCacheLocked(
            DataField::auxiliaryTraits, &QnPtzData::auxiliaryTraits, data.auxiliaryTraits);
    }

    return changedFields;
}
