// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recording_period.h"

#include <core/resource/camera_resource.h>
#include <nx/vms/api/data/camera_attributes_data.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/system_context.h>

namespace {

using namespace nx::vms::client::desktop;
using Storage = UserEditableMultiple<std::chrono::seconds>;

static constexpr auto kDifferentValueAutoMode =
    std::chrono::seconds(std::numeric_limits<std::chrono::seconds::rep>::min());

static constexpr auto kDifferentValueManualMode =
    std::chrono::seconds(std::numeric_limits<std::chrono::seconds::rep>::max());

bool hasPeriodValue(std::chrono::seconds value)
{
    return value != kDifferentValueAutoMode && value != kDifferentValueManualMode;
}

bool isNegative(std::chrono::seconds value)
{
    return value.count() < 0;
}

std::chrono::seconds makeAutoStateValue(std::chrono::seconds duration)
{
    return std::chrono::seconds(-std::llabs(duration.count()));
}

Storage extractValue(
    std::chrono::seconds defaultPeriodValue,
    const QnVirtualCameraResourceList& cameras,
    std::function<std::chrono::seconds(const QnVirtualCameraResourcePtr& camera)> getter)
{
    if (cameras.empty())
        return Storage::fromValue(makeAutoStateValue(defaultPeriodValue));

    Storage result;
    bool samePeriod = true;
    const auto firstPeriodValue = getter(cameras.first());
    const bool firstPeriodValueIsNegative = isNegative(firstPeriodValue);
    for (auto it = cameras.begin() + 1; it != cameras.end(); ++it)
    {
        const auto cameraPeriod = getter(*it);
        if (firstPeriodValue == cameraPeriod)
            continue;

        samePeriod = false;
        if (firstPeriodValueIsNegative != isNegative(cameraPeriod))
            return result;
    }

    if (samePeriod)
        result.setBase(firstPeriodValue);
    else if (firstPeriodValueIsNegative)
        result.setBase(kDifferentValueAutoMode);
    else
        result.setBase(kDifferentValueManualMode);

    return result;
}

} // namespace

namespace nx::vms::client::desktop {

RecordingPeriod::RecordingPeriod(
    std::chrono::seconds defaultPeriodValue,
    const PeriodStorage& value)
    :
    m_defaultPeriodValue(defaultPeriodValue),
    m_value(value)
{
}

RecordingPeriod RecordingPeriod::minPeriod(std::chrono::seconds duration)
{
    return RecordingPeriod(
        nx::vms::api::kDefaultMinArchivePeriod,
        PeriodStorage::fromValue(duration));
}

RecordingPeriod RecordingPeriod::minPeriod(const QnVirtualCameraResourceList& cameras)
{
    const auto value = extractValue(nx::vms::api::kDefaultMinArchivePeriod, cameras,
        [](const QnVirtualCameraResourcePtr& camera) { return camera->minPeriod(); });

    return RecordingPeriod(nx::vms::api::kDefaultMinArchivePeriod, value);
}

RecordingPeriod RecordingPeriod::maxPeriod(std::chrono::seconds duration)
{
    return RecordingPeriod(
        nx::vms::api::kDefaultMaxArchivePeriod,
        PeriodStorage::fromValue(duration));
}

RecordingPeriod RecordingPeriod::maxPeriod(const QnVirtualCameraResourceList& cameras)
{
    const auto value = extractValue(nx::vms::api::kDefaultMaxArchivePeriod, cameras,
        [](const QnVirtualCameraResourcePtr& camera) { return camera->maxPeriod(); });

    auto result = RecordingPeriod(nx::vms::api::kDefaultMaxArchivePeriod, value);

    using namespace nx::vms::api;
    using namespace std::chrono;
    if (!cameras.empty())
    {
        const auto saasManager = cameras[0]->systemContext()->saasServiceManager();
        const days maxDays(saasManager->tierLimit(SaasTierLimitName::maxArchiveDays).value_or(0));
        result.setForcedMaxValue(duration_cast<seconds>(maxDays));
    }

    return result;
}

Qt::CheckState RecordingPeriod::autoCheckState() const
{
    if (m_forcedMaxValue.count() > 0)
        return Qt::Unchecked;

    if (!m_value.hasValue())
        return Qt::PartiallyChecked;

    return isNegative(m_value.get())
        ? Qt::Checked
        : Qt::Unchecked;
}

bool RecordingPeriod::isManualMode() const
{
    if (m_forcedMaxValue.count() > 0)
        return true;
    return m_value.hasValue() && !isNegative(m_value.get());
}

bool RecordingPeriod::hasManualPeriodValue() const
{
    return isManualMode() && hasPeriodValue(m_value.get());
}

bool RecordingPeriod::isApplicable() const
{
    const auto autoState = autoCheckState();
    if (autoState == Qt::PartiallyChecked)
        return false;

    return autoState == Qt::Checked || hasManualPeriodValue();
}

std::chrono::seconds RecordingPeriod::displayValue() const
{
    return (m_value.hasValue() && hasPeriodValue(m_value.get()))
        ? std::chrono::seconds(std::llabs(m_value.get().count()))
        : m_defaultPeriodValue;
}

std::chrono::seconds RecordingPeriod::rawValue() const
{
    return m_value.valueOr(m_defaultPeriodValue);
}

void RecordingPeriod::setAutoMode()
{
    const auto targetValue = makeAutoStateValue(displayValue());
    m_value.setUser(targetValue);
}

void RecordingPeriod::setManualMode()
{
    setManualModeWithPeriod(displayValue());
}

void RecordingPeriod::setManualModeWithPeriod(std::chrono::seconds duration)
{
    m_value.setUser(duration);
}

std::chrono::seconds RecordingPeriod::forcedMaxValue() const
{
    return m_forcedMaxValue;
}

void RecordingPeriod::setForcedMaxValue(std::chrono::seconds value)
{
    m_forcedMaxValue = value;
}

} // namespace nx::vms::client::desktop
