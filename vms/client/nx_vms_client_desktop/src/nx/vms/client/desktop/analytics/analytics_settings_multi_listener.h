#pragma once

#include <QtCore/QObject>
#include <QtCore/QJsonObject>

#include <core/resource/resource_fwd.h>

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

    QJsonObject values(const QnUuid& engineId) const;
    QJsonObject model(const QnUuid& engineId) const;

    QSet<QnUuid> engineIds() const;

signals:
    void valuesChanged(const QnUuid& engineId, const QJsonObject& values);
    void modelChanged(const QnUuid& engineId, const QJsonObject& values);

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace nx::vms::client::desktop
