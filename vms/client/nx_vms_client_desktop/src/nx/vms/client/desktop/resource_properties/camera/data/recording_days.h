#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/common/redux/redux_types.h>

namespace nx::vms::client::desktop {

class RecordingDays
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
    int days() const;

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
