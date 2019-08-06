#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <api/server_rest_connection_fwd.h>

class QnUuid;

class QnCommonModule;
class QnResourceAccessManager;

namespace nx::vms::event { class RuleManager; }

namespace nx::vms::client::core {

class UserWatcher;
class OrderedRequestsManager;

} // namespace nx::vms::client::core

namespace nx::client::mobile {

class SoftwareTriggersController: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId
        NOTIFY resourceIdChanged)

public:
    SoftwareTriggersController(QObject* parent = nullptr);
    virtual ~SoftwareTriggersController() override;

    static void registerQmlType();

    QString resourceId() const;
    void setResourceId(const QString& id);

    Q_INVOKABLE bool activateTrigger(const QnUuid& id);
    Q_INVOKABLE bool deactivateTrigger();
    Q_INVOKABLE void cancelTriggerAction();
    Q_INVOKABLE QnUuid activeTriggerId() const;
    Q_INVOKABLE bool hasActiveTrigger() const;

signals:
    void resourceIdChanged();

    void triggerActivated(const QnUuid& id, bool success);
    void triggerDeactivated(const QnUuid& id);
    void triggerCancelled(const QnUuid& id);

private:
    bool setTriggerState(QnUuid id, vms::event::EventState state);
    void setActiveTrigger(const QnUuid& id);

private:
    const QScopedPointer<nx::vms::client::core::OrderedRequestsManager> m_requestsManager;
    QnCommonModule* const m_commonModule = nullptr;
    nx::vms::client::core::UserWatcher* const m_userWatcher = nullptr;
    QnResourceAccessManager* const m_accessManager = nullptr;
    vms::event::RuleManager* const m_ruleManager = nullptr;

    QnUuid m_resourceId;
    QnUuid m_activeTriggerId;
};

} // namespace nx::client::mobile
