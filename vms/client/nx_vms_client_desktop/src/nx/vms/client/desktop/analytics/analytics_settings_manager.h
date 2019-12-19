#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QJsonObject>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

class AnalyticsSettingsManager;

struct DeviceAgentId
{
    QnUuid device;
    QnUuid engine;

    inline bool operator==(const DeviceAgentId& other) const
    {
        return device == other.device && engine == other.engine;
    }
};

uint qHash(const DeviceAgentId& key);

class AnalyticsSettingsListener: public QObject
{
    Q_OBJECT

public:
    const DeviceAgentId agentId;

    QJsonObject model() const;
    QJsonObject values() const;

signals:
    void modelChanged(const QJsonObject& model);
    void valuesChanged(const QJsonObject& values);

protected:
    AnalyticsSettingsListener(const DeviceAgentId& agentId, AnalyticsSettingsManager* manager);

private:
    AnalyticsSettingsManager* m_manager = nullptr;

    friend class AnalyticsSettingsManager;
};

using AnalyticsSettingsListenerPtr = std::shared_ptr<AnalyticsSettingsListener>;

class AnalyticsSettingsManager: public QObject
{
    Q_OBJECT

public:
    enum class Error
    {
        noError,
        busy,
    };

    AnalyticsSettingsManager(QObject* parent = nullptr);
    virtual ~AnalyticsSettingsManager() override;

    AnalyticsSettingsListenerPtr getListener(const DeviceAgentId& agentId);

    QJsonObject values(const DeviceAgentId& agentId) const;
    QJsonObject model(const DeviceAgentId& agentId) const;

    Error setValues(const QHash<DeviceAgentId, QJsonObject>& valuesByAgentId);

    bool isApplyingChanges() const;

signals:
    void appliedChanges();

private:
    class Private;
    QScopedPointer<Private> d;

    friend class AnalyticsSettingsListener;
};

} // namespace nx::vms::client::desktop
