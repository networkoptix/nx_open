// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>

#include <QtCore/QObject>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_types.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;

class DeviceAgentSettingsAdapter: public QObject, public WindowContextAware
{
    using base_type = QObject;

public:
    DeviceAgentSettingsAdapter(
        CameraSettingsDialogStore* store,
        WindowContext* context,
        QWidget* parent = nullptr);

    virtual ~DeviceAgentSettingsAdapter() override;

    void setCamera(const QnVirtualCameraResourcePtr& camera);

    void applySettings();

    /**
     * Refresh all analytics settings for the selected camera in case they were changed externally.
     */
    void refreshSettings();

    /**
     * Actual data for the currently selected camera.
     */
    std::unordered_map<QnUuid, DeviceAgentData> dataByEngineId() const;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
