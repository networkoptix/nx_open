// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/client/core/analytics/analytics_engine_info.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API AnalyticsEnginesWatcher: public QObject
{
    Q_OBJECT

    using base_type = QObject;

public:
    AnalyticsEnginesWatcher(QObject* parent = nullptr);
    virtual ~AnalyticsEnginesWatcher() override;

    QHash<QnUuid, AnalyticsEngineInfo> engineInfos() const;
    AnalyticsEngineInfo engineInfo(const QnUuid& engineId) const;

signals:
    void engineAdded(const QnUuid& engineId, const AnalyticsEngineInfo& info);
    void engineRemoved(const QnUuid& engineId);
    void engineUpdated(const QnUuid& engineId);
    void engineSettingsModelChanged(const QnUuid& engineId);

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::core
