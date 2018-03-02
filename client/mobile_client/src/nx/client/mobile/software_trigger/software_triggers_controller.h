#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <api/server_rest_connection_fwd.h>

class QnUuid;

class QnUserWatcher;
class QnCommonModule;
class QnResourceAccessManager;

namespace nx {

namespace vms {
namespace event {

class RuleManager;

} // namespace event
} // namespace vms

namespace client {
namespace mobile {

class SoftwareTriggersController: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QString resourceId READ resourceId WRITE setResourceId
        NOTIFY resourceIdChanged)

public:
    SoftwareTriggersController(QObject* parent = nullptr);

    QString resourceId() const;
    void setResourceId(const QString& id);

    Q_INVOKABLE bool activateTrigger(const QnUuid& id, bool prolonged);
    Q_INVOKABLE void cancelTriggerAction(const QnUuid& id);

signals:
    void resourceIdChanged();

    void triggerExecuted(const QnUuid& id, bool success);

private:
    void cancelAllTriggers();

private:
    using IdToHandleHash = QHash<QnUuid, rest::Handle>;
    using HandleToIdHash = QHash<rest::Handle, QnUuid>;

    QnCommonModule* const m_commonModule = nullptr;
    QnUserWatcher* const m_userWatcher = nullptr;
    QnResourceAccessManager* const m_accessManager = nullptr;
    vms::event::RuleManager* const m_ruleManager = nullptr;


    QnUuid m_resourceId;
    IdToHandleHash m_idToHandle;
    HandleToIdHash m_handleToId;
};

} // namespace mobile
} // namespace client
} // namespace nx
