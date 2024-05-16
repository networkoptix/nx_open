// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QObject>

#include <nx/vms/api/analytics/analytics_actions.h>
#include <core/resource/resource_fwd.h>

#include "analytics_settings_types.h"

namespace nx::vms::client::core {

class AnalyticsSettingsManager;

class NX_VMS_CLIENT_CORE_API AnalyticsSettingsMultiListener: public QObject
{
    Q_OBJECT

public:
    enum class ListenPolicy
    {
        allEngines,
        enabledEngines,
    };

    AnalyticsSettingsMultiListener(
        AnalyticsSettingsManager* manager,
        const QnVirtualCameraResourcePtr& camera,
        ListenPolicy listenPolicy,
        QObject* parent = nullptr);
    virtual ~AnalyticsSettingsMultiListener() override;

    DeviceAgentData data(const nx::Uuid& engineId) const;

    QSet<nx::Uuid> engineIds() const;

signals:
    void enginesChanged();
    void dataChanged(const nx::Uuid& engineId, const DeviceAgentData& data);
    void previewDataReceived(const nx::Uuid& engineId, const DeviceAgentData& data);
    void actionResultReceived(
        const nx::Uuid& engineId, const api::AnalyticsActionResult& result);

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace nx::vms::client::core
