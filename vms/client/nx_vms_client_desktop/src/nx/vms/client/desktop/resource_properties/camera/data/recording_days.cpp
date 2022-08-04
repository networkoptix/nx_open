// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recording_days.h"

#include <core/resource/camera_resource.h>
#include <nx/vms/api/data/camera_attributes_data.h>

namespace {

using namespace nx::vms::client::desktop;
using Storage = UserEditableMultiple<int>;

static constexpr int kDifferentValueAutoMode = std::numeric_limits<int>::min();
static constexpr int kDifferentValueManualMode = std::numeric_limits<int>::max();

bool hasDaysValue(int value)
{
    return value != kDifferentValueAutoMode && value != kDifferentValueManualMode;
}

bool isNegative(int value)
{
    return value < 0;
}

int makeAutoStateValue(int days)
{
    return -std::abs(days);
}

Storage extractValue(
    int emptyDaysValue,
    const QnVirtualCameraResourceList& cameras,
    std::function<int (const QnVirtualCameraResourcePtr& camera)> getter)
{
    if (cameras.empty())
        return Storage::fromValue(makeAutoStateValue(emptyDaysValue));

    Storage result;
    bool sameDays = true;
    const int firstDaysValue = getter(cameras.first());
    const bool firstDaysValueIsNegative = isNegative(firstDaysValue);
    for (auto it = cameras.begin() + 1; it != cameras.end(); ++it)
    {
        const int cameraDays = getter(*it);
        if (firstDaysValue == cameraDays)
            continue;

        sameDays = false;
        if (firstDaysValueIsNegative != isNegative(cameraDays))
            return result;
    }

    if (sameDays)
        result.setBase(firstDaysValue);
    else if (firstDaysValueIsNegative)
        result.setBase(kDifferentValueAutoMode);
    else
        result.setBase(kDifferentValueManualMode);

    return result;
}

} // namespace

namespace nx::vms::client::desktop {

RecordingDays::RecordingDays(
    int emptyDaysValue,
    const DaysStorage& value)
    :
    m_emptyDaysValue(emptyDaysValue),
    m_value(value)
{
}

RecordingDays RecordingDays::minDays(int daysCount)
{
    return RecordingDays(
        duration_cast<std::chrono::days>(nx::vms::api::kDefaultMinArchivePeriod).count(),
        DaysStorage::fromValue(daysCount));
}

RecordingDays RecordingDays::minDays(
    const QnVirtualCameraResourceList& cameras)
{
    static const auto defaultMinDays =
        duration_cast<std::chrono::days>(nx::vms::api::kDefaultMinArchivePeriod).count();
    const auto value = extractValue(
        defaultMinDays, cameras,
        [](const QnVirtualCameraResourcePtr& camera) 
        { 
            return duration_cast<std::chrono::days>(camera->minPeriod()).count();
        });

    return RecordingDays(defaultMinDays, value);
}

RecordingDays RecordingDays::maxDays(int daysCount)
{
    return RecordingDays(
        duration_cast<std::chrono::days>(nx::vms::api::kDefaultMaxArchivePeriod).count(),
        DaysStorage::fromValue(daysCount));
}

RecordingDays RecordingDays::maxDays(
    const QnVirtualCameraResourceList& cameras)
{
    static const auto defaultMaxDays =
        duration_cast<std::chrono::days>(nx::vms::api::kDefaultMaxArchivePeriod).count();

    const auto value = extractValue(
        defaultMaxDays,
        cameras,
        [](const QnVirtualCameraResourcePtr& camera) 
        { 
            return duration_cast<std::chrono::days>(camera->maxPeriod()).count();
        });

    return RecordingDays(defaultMaxDays, value);
}

Qt::CheckState RecordingDays::autoCheckState() const
{
    if (!m_value.hasValue())
        return Qt::PartiallyChecked;

    return m_value.get() < 0
        ? Qt::Checked
        : Qt::Unchecked;
}

bool RecordingDays::isManualMode() const
{
    return m_value.hasValue() && m_value.get() >= 0;
}

bool RecordingDays::hasManualDaysValue() const
{
    return isManualMode() && hasDaysValue(m_value.get());
}

bool RecordingDays::isApplicable() const
{
    const auto autoState = autoCheckState();
    if (autoState == Qt::PartiallyChecked)
        return false;

    return autoState == Qt::Checked || hasManualDaysValue();
}

int RecordingDays::displayValue() const
{
    return m_value.hasValue()
        ? std::abs(m_value.get())
        : m_emptyDaysValue;
}

std::chrono::seconds RecordingDays::rawValue() const
{
    return std::chrono::days(m_value.valueOr(m_emptyDaysValue));
}

void RecordingDays::setAutoMode()
{
    const auto targetValue = makeAutoStateValue(displayValue());
    m_value.setUser(targetValue);
}

void RecordingDays::setManualMode()
{
    setManualModeWithDays(displayValue());
}

void RecordingDays::setManualModeWithDays(int daysCount)
{
    m_value.setUser(daysCount);
}

} // namespace nx::vms::client::desktop
