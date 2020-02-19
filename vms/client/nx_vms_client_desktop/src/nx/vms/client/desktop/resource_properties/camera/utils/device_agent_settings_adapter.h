#pragma once

#include <unordered_map>

#include <QtCore/QObject>

#include <nx/vms/client/desktop/analytics/analytics_settings_types.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;

class DeviceAgentSettingsAdapter: public QObject
{
    using base_type = QObject;

public:
    DeviceAgentSettingsAdapter(CameraSettingsDialogStore* store, QObject* parent = nullptr);
    virtual ~DeviceAgentSettingsAdapter() override;

    void setCamera(const QnVirtualCameraResourcePtr& camera);

    void applySettings();

    /**
     * Actual data for the currently selected camera.
     */
    std::unordered_map<QnUuid, DeviceAgentData> dataByEngineId() const;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
