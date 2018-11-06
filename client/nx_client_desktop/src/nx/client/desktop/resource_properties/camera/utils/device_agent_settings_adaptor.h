#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::client::desktop {

class CameraSettingsDialogStore;

class DeviceAgentSettingsAdaptor: public QObject
{
    using base_type = QObject;

public:
    DeviceAgentSettingsAdaptor(CameraSettingsDialogStore* store, QObject* parent = nullptr);
    virtual ~DeviceAgentSettingsAdaptor() override;

    void setCamera(const QnVirtualCameraResourcePtr& camera);

    void refreshSettings(const QnUuid& engineId, bool forceRefresh = false);
    void applySettings();

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::client::desktop
