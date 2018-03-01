#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>
#include <nx/vms/event/event_fwd.h>

class QnCommonModule;

namespace nx {

namespace vms {
namespace event {

class RuleManager;

} // namespace event
} // namespace vms

namespace client {
namespace mobile {

struct SoftwareTriggerData
{
    QString triggerId;
    QString name;
    bool prolonged = false;
};

// TODO: add check if resource is camera
class SoftwareTriggersWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    SoftwareTriggersWatcher(QObject* parent = nullptr);

    void setResourceId(const QnUuid& resourceId);

    void updateTriggersAvailability();


    bool triggerEnabled(const QnUuid& id) const;
    QString triggerIcon(const QnUuid& id) const;
    SoftwareTriggerData triggerData(const QnUuid& id) const;

signals:
    void resourceIdChanged();

    void triggerRemoved(const QnUuid& id);
    void triggerAdded(
        const QnUuid& id,
        const SoftwareTriggerData& triggerData,
        const QString& iconPath,
        bool triggerEnabled);

    void triggerEnabledChanged(const QnUuid& id);
    void triggerIconChanged(const QnUuid& id);
    void triggerNameChanged(const QnUuid& id);
    void triggerIdChanged(const QnUuid& id);
    void triggerProlongedChanged(const QnUuid& id);

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
