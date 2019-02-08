#pragma once

#include <QtCore/QObject>

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

    void refreshSettings(const QnUuid& engineId, bool forceRefresh = false);
    void applySettings();

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
