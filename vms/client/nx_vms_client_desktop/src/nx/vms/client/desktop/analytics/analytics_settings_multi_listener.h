#pragma once

#include <QtCore/QObject>
#include <QtCore/QJsonObject>

#include <core/resource/resource_fwd.h>

#include "analytics_settings_types.h"

namespace nx::vms::client::desktop {

class AnalyticsSettingsMultiListener: public QObject
{
    Q_OBJECT

public:
    enum class ListenPolicy
    {
        allEngines,
        enabledEngines,
    };

    AnalyticsSettingsMultiListener(
        const QnVirtualCameraResourcePtr& camera,
        ListenPolicy listenPolicy,
        QObject* parent = nullptr);
    virtual ~AnalyticsSettingsMultiListener() override;

    DeviceAgentData data(const QnUuid& engineId) const;

    QSet<QnUuid> engineIds() const;

signals:
    void enginesChanged();
    void dataChanged(const QnUuid& engineId, const DeviceAgentData& data);

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace nx::vms::client::desktop
