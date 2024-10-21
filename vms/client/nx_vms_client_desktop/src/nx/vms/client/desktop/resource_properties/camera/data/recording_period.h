// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/common/flux/flux_types.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API RecordingPeriod
{
public:
    static RecordingPeriod minPeriod(std::chrono::seconds duration);
    static RecordingPeriod minPeriod(
        const QnVirtualCameraResourceList& cameras = QnVirtualCameraResourceList());

    static RecordingPeriod maxPeriod(std::chrono::seconds duration);
    static RecordingPeriod maxPeriod(
        const QnVirtualCameraResourceList& cameras = QnVirtualCameraResourceList());

    Qt::CheckState autoCheckState() const;
    bool isManualMode() const;
    bool hasManualPeriodValue() const;

    // Checks if value can be saved to a camera. Value can't be saved if:
    // - Instance represents multiple recording periods values with different auto modes.
    // - Instance represents manual mode with different period values.
    bool isApplicable() const;

    /**
     * Positive duration time period, used to display in the UI.
     */
    std::chrono::seconds displayValue() const;

    /**
     * Raw value, used to store value in the database. Positive if limit is enabled, negative
     * otherwise (absolute value is used to keep value when it is enabled / disabled).
     */
    std::chrono::seconds rawValue() const;

    void setAutoMode();
    void setManualMode();
    void setManualModeWithPeriod(std::chrono::seconds duration);

    std::chrono::seconds forcedMaxValue() const;
    void setForcedMaxValue(std::chrono::seconds value);

private:
    using PeriodStorage = UserEditableMultiple<std::chrono::seconds>;

    RecordingPeriod(
        std::chrono::seconds defaultPeriodValue,
        const PeriodStorage& value);

    std::chrono::seconds m_defaultPeriodValue{0};
    PeriodStorage m_value;
    std::chrono::seconds m_forcedMaxValue{};
};

} // namespace nx::vms::client::desktop
