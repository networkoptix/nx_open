// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/client/core/analytics/analytics_engine_info.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;

class CameraSettingsAnalyticsEnginesWatcherInterface
{
public:
    virtual ~CameraSettingsAnalyticsEnginesWatcherInterface() = default;

    virtual QList<core::AnalyticsEngineInfo> engineInfoList() const = 0;
    virtual nx::vms::api::StreamIndex analyzedStreamIndex(const QnUuid& engineId) const = 0;
};

class CameraSettingsAnalyticsEnginesWatcher:
    public QObject,
    public CameraSettingsAnalyticsEnginesWatcherInterface,
    public QnWorkbenchContextAware
{
    using base_type = QObject;

public:
    CameraSettingsAnalyticsEnginesWatcher(
        CameraSettingsDialogStore* store, QObject* parent = nullptr);
    virtual ~CameraSettingsAnalyticsEnginesWatcher() override;

    QnVirtualCameraResourcePtr camera() const;
    void setCamera(const QnVirtualCameraResourcePtr& camera);

    virtual QList<core::AnalyticsEngineInfo> engineInfoList() const override;
    virtual nx::vms::api::StreamIndex analyzedStreamIndex(const QnUuid& engineId) const override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
