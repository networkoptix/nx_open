// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/resource_properties/camera/data/analytics_engine_info.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class AnalyticsEnginesWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

    using base_type = QObject;

public:
    AnalyticsEnginesWatcher(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~AnalyticsEnginesWatcher() override;

    QHash<nx::Uuid, AnalyticsEngineInfo> engineInfos() const;
    AnalyticsEngineInfo engineInfo(const nx::Uuid& engineId) const;

signals:
    void engineAdded(const nx::Uuid& engineId, const AnalyticsEngineInfo& info);
    void engineRemoved(const nx::Uuid& engineId);
    void engineUpdated(const nx::Uuid& engineId);
    void engineSettingsModelChanged(const nx::Uuid& engineId);

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
