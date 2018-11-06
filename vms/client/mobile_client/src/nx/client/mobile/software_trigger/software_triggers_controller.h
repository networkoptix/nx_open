#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <api/server_rest_connection_fwd.h>

class QnUuid;

class QnCommonModule;
class QnResourceAccessManager;

namespace nx {

namespace vms {
namespace event {

class RuleManager;

} // namespace event
} // namespace vms

namespace client {

namespace core {

class UserWatcher;

} // namespace core

namespace mobile {

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

private:
    QnCommonModule* const m_commonModule = nullptr;
    core::UserWatcher* const m_userWatcher = nullptr;
    QnResourceAccessManager* const m_accessManager = nullptr;
    vms::event::RuleManager* const m_ruleManager = nullptr;

    QnUuid m_resourceId;
    QnUuid m_activeTriggerId;
};

} // namespace mobile
} // namespace client
} // namespace nx
