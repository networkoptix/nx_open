// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/common/flux/flux_types.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API RecordingDays
{
public:
    static RecordingDays minDays(int daysCount);
    static RecordingDays minDays(
        const QnVirtualCameraResourceList& cameras = QnVirtualCameraResourceList());
    static RecordingDays maxDays(int daysCount);
    static RecordingDays maxDays(
        const QnVirtualCameraResourceList& cameras = QnVirtualCameraResourceList());

    Qt::CheckState autoCheckState() const;
    bool isManualMode() const;
    bool hasManualDaysValue() const;

    // Checks if value can be saved to camera.
    // We can't save value to camera if instance represents multiple recording
    // days value with different auto modes.
    // Also, we can't save data to camera if it represents manual mode with
    // different days values.
    bool isApplicable() const;

    /**
     * Positive count of days, used to display in the UI.
     */
    int displayValue() const;

    /**
     * Raw value, used to store value in database. Positive if days limit is enabled, negative
     * otherwise (absolute value is used to keep value when it is enabled / disabled).
     */
    std::chrono::seconds rawValue() const;

    void setAutoMode();
    void setManualMode();
    void setManualModeWithDays(int daysCount);

private:
    using DaysStorage = UserEditableMultiple<int>;

    RecordingDays(
        int emptyDaysValue,
        const DaysStorage& value);

    int m_emptyDaysValue = 0;
    DaysStorage m_value;
};

} // namespace nx::vms::client::desktop
