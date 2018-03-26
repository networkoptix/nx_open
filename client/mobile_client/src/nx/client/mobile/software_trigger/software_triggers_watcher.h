#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/event/event_fwd.h>
#include <client_core/connection_context_aware.h>

class QnCommonModule;

namespace nx {

namespace vms {
namespace event {

class RuleManager;

} // namespace event
} // namespace vms

namespace client {
namespace mobile {

class SoftwareTriggersWatcher: public QObject, public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum TriggerField
    {
        NoField = 0,
        EnabledField = 0x1,
        ProlongedField = 0x2,
        NameField = 0x4,
        IconField = 0x8
    };

    Q_DECLARE_FLAGS(TriggerFields, TriggerField)

    SoftwareTriggersWatcher(QObject* parent = nullptr);

    void setResourceId(const QnUuid& resourceId);

    void updateTriggersAvailability();

    bool triggerEnabled(const QnUuid& id) const;
    bool prolongedTrigger(const QnUuid& id) const;
    QString triggerName(const QnUuid& id) const;
    QString triggerIcon(const QnUuid& id) const;

signals:
    void resourceIdChanged();

    void triggerRemoved(const QnUuid& id);
    void triggerAdded(
        const QnUuid& id,
        const QString& iconPath,
        const QString& name,
        bool prolonged,
        bool enabled);

    void triggerFieldsChanged(const QnUuid& id, TriggerFields fields);

private:
    void updateTriggers();
    void tryRemoveTrigger(const QnUuid& id);

    void updateTriggerByRule(const nx::vms::event::RulePtr& rule);

    void updateTriggerAvailability(const QnUuid& id, bool emitSignal);
    void updateTriggerData(const QnUuid& id, bool emitSignal);

private:
    struct Description;
    using DescriptionPtr = QSharedPointer<Description>;
    using DescriptionsHash = QHash<QnUuid, DescriptionPtr>;

private:
    QnCommonModule* const m_commonModule = nullptr;
    vms::event::RuleManager* const m_ruleManager = nullptr;

    QnUuid m_resourceId;
    DescriptionsHash m_data;
};

} // namespace mobile
} // namespace client
} // namespace nx
