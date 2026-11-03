// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "caching_ptz_controller.h"

#include <core/resource/resource.h>
#include <nx/utils/algorithm/index_of.h>
#include <utils/common/delayed.h>

using namespace nx::vms::common::ptz;

namespace nx::vms::client::core {
namespace ptz {

namespace {
static constexpr auto kUpdateInterval = std::chrono::minutes(1);
} // namespace

CachingPtzController::CachingPtzController(const QnPtzControllerPtr& baseController):
    base_type(baseController),
    m_initialized(false)
{
    invalidate();
}

CachingPtzController::~CachingPtzController()
{
}

bool CachingPtzController::extends(Ptz::Capabilities capabilities)
{
    return capabilities.testFlag(Ptz::Capability::asynchronous)
        && !capabilities.testFlag(Ptz::Capability::synchronized);
}

Ptz::Capabilities CachingPtzController::getCapabilities(
    const Options& options) const
{
    const Ptz::Capabilities capabilities = base_type::getCapabilities(options);
    return extends(capabilities)
        ? capabilities | Ptz::Capability::synchronized
        : capabilities;
}

bool CachingPtzController::getLimits(
    nx::vms::api::PtzPositionLimits* limits,
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

bool CachingPtzController::getFlip(
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

template<class T, class Request>
bool CachingPtzController::getCachedData(T* value,
    DataField field,
    T QnPtzData::* member,
    UpdateTimerType timerType,
    Request request) const
{
    bool result = false;
    bool requestFromBase = false;
    auto& timer = m_updateTimers[timerType];

    {
        const NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_data.fields.testFlag(field))
        {
            *value = m_data.*member;
            result = true;
        }

        if (!result || timer.hasExpired())
        {
            timer.setRemainingTime(kUpdateInterval);
            requestFromBase = true;
        }
    }

    if (requestFromBase)
    {
        const bool accepted = request();
        if (!accepted)
        {
            const NX_MUTEX_LOCKER locker(&m_mutex);
            timer.forceExpire();
        }
        result = accepted;
    }

    return result;
}

bool CachingPtzController::getPresets(QnPtzPresetList* presets) const
{
    return getCachedData(presets,
        DataField::presets,
        &QnPtzData::presets,
        UpdateTimerType::presets,
        [this, presets]() { return base_type::getPresets(presets); });
}

bool CachingPtzController::getPresetLimits(nx::vms::api::PtzPresetLimits* limits) const
{
    {
        const NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_presetLimits)
        {
            *limits = *m_presetLimits;
            return true;
        }

        if (m_presetLimitsPending)
            return false;

        m_presetLimitsPending = true;
    }

    if (!base_type::getPresetLimits(limits))
    {
        const NX_MUTEX_LOCKER locker(&m_mutex);
        m_presetLimitsPending = false;
        m_discardPresetLimitsResponse = false;
    }

    return false;
}

bool CachingPtzController::getTours(QnPtzTourList* tours) const
{
    return getCachedData(tours,
        DataField::tours,
        &QnPtzData::tours,
        UpdateTimerType::tours,
        [this, tours]() { return base_type::getTours(tours); });
}

bool CachingPtzController::getActiveObject(QnPtzObject* activeObject) const
{
    if (!base_type::getActiveObject(activeObject))
        return false;

    const NX_MUTEX_LOCKER locker(&m_mutex);
    if (!m_data.fields.testFlag(DataField::activeObject))
        return false;

    *activeObject = m_data.activeObject;
    return true;
}

bool CachingPtzController::getHomeObject(QnPtzObject* homeObject) const
{
    if (!base_type::getHomeObject(homeObject))
        return false;

    const NX_MUTEX_LOCKER locker(&m_mutex);
    if (!m_data.fields.testFlag(DataField::homeObject))
        return false;

    *homeObject = m_data.homeObject;
    return true;
}

bool CachingPtzController::getAuxiliaryTraits(
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

bool CachingPtzController::getData(
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

void CachingPtzController::baseFinished(Command command, const QVariant& data)
{
    DataFields changedFields = DataField::none;

    if (command == Command::getPresetLimits)
    {
        bool accepted = false;
        {
            const NX_MUTEX_LOCKER locker(&m_mutex);
            accepted = m_presetLimitsPending && !m_discardPresetLimitsResponse && data.isValid();
            if (accepted)
                m_presetLimits = data.value<nx::vms::api::PtzPresetLimits>();

            m_presetLimitsPending = false;
            m_discardPresetLimitsResponse = false;
        }
        base_type::baseFinished(command, accepted ? data : QVariant());
        return;
    }

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

bool CachingPtzController::initializeInternal()
{
    /* Note that this field is accessed from this object's thread only,
     * so there is no need to lock. */
    if (m_initialized)
        return true;

    QnPtzData data;
    return getData(&data, DataField::all, {Type::operational});
}

void CachingPtzController::initialize()
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
            {
                controller->invalidatePresetLimits();
                controller->initializeInternal();
            }
        };

    connect(resource().get(), &QnResource::statusChanged, this, safeInitialize);
    connect(resource().get(), &QnResource::parentIdChanged, this, safeInitialize);

    // Do not bootstrap the cache eagerly by calling initializeInternal here: on a system with
    // many PTZ cameras this would fire GetDataPtzCommand for every camera at once on login,
    // saturating the HTTP client pool and stalling user-initiated PTZ commands by tens of
    // seconds. Data is fetched lazily on first access via getData/getPresets/getTours and
    // refreshed on statusChanged.
}

template<class T>
DataFields CachingPtzController::updateCacheLocked(
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
DataFields CachingPtzController::updateCacheLocked(
    DataField field, T QnPtzData::*member, const QVariant& value)
{
    return updateCacheLocked(field, member, value.value<T>());
}

DataFields CachingPtzController::updateCacheLocked(const QnPtzData& data)
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

void CachingPtzController::invalidatePresetLimits()
{
    const NX_MUTEX_LOCKER locker(&m_mutex);
    m_presetLimits.reset();
    // Drain the old request before starting another: completions carry no request id.
    m_discardPresetLimitsResponse = m_presetLimitsPending;
}

void CachingPtzController::invalidate()
{
    invalidatePresetLimits();
    {
        const NX_MUTEX_LOCKER locker(&m_mutex);

        for (auto& timer: m_updateTimers)
            timer.forceExpire();
    }

    base_type::invalidate();
}

} // namespace ptz
} // namespace nx::vms::client::core
