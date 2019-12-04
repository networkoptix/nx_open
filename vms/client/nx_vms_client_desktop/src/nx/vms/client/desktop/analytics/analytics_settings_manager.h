#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QJsonObject>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

class AnalyticsSettingsManager;

class AnalyticsSettingsListener: public QObject
{
    Q_OBJECT

public:
    const QnUuid deviceId;
    const QnUuid engineId;

    QJsonObject model() const;
    QJsonObject values() const;

signals:
    void modelChanged(const QJsonObject& model);
    void valuesChanged(const QJsonObject& values);

protected:
    AnalyticsSettingsListener(
        const QnUuid& deviceId, const QnUuid& engineId, AnalyticsSettingsManager* cache);

private:
    AnalyticsSettingsManager* m_cache = nullptr;

    friend class AnalyticsSettingsManager;
};

using AnalyticsSettingsListenerPtr = std::shared_ptr<AnalyticsSettingsListener>;

class AnalyticsSettingsManager: public QObject
{
    Q_OBJECT

public:
    AnalyticsSettingsManager(QObject* parent = nullptr);
    virtual ~AnalyticsSettingsManager() override;

    AnalyticsSettingsListenerPtr getListener(const QnUuid& deviceId, const QnUuid& engineId);

protected:
    QJsonObject values(const QnUuid& deviceId, const QnUuid& engineId) const;
    QJsonObject model(const QnUuid& deviceId, const QnUuid& engineId) const;

private:
    class Private;
    QScopedPointer<Private> d;

    friend class AnalyticsSettingsListener;
};

} // namespace nx::vms::client::desktop
