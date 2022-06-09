// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::vms::rules { class Rule;}

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API SoftwareTriggersWatcher:
    public QObject,
    public SystemContextAware
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

    SoftwareTriggersWatcher(
        SystemContext* context,
        QObject* parent = nullptr);

    void setResourceId(const QnUuid& resourceId);

    void updateTriggersAvailability();
    void updateTriggerAvailability(QnUuid id);

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
    struct Description;
    using DescriptionPtr = QSharedPointer<Description>;
    using DescriptionsHash = QHash<QnUuid, DescriptionPtr>;

private:
    DescriptionPtr findTrigger(QnUuid id) const;
    void tryRemoveTrigger(const QnUuid& id);

    void updateTriggers();
    void updateTriggerByData(QnUuid id, const DescriptionPtr& newData);

    void updateTriggerByRule(const nx::vms::event::RulePtr& rule);

    void updateTriggerByVmsRule(const nx::vms::rules::Rule* rule);

private:
    QnUuid m_resourceId;
    QnMediaServerResourcePtr m_server;
    DescriptionsHash m_data;
};

} // namespace nx::vms::client::core
