// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>
#include <nx/vms/event/event_fwd.h>

namespace nx::vms::rules { class Rule;}

namespace nx::vms::client::core {

class ServerResource;
using ServerResourcePtr = QnSharedResourcePointer<ServerResource>;

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

    virtual ~SoftwareTriggersWatcher() override;

    void setResourceId(const nx::Uuid& resourceId);

    void updateTriggersAvailability();
    void updateTriggerAvailability(nx::Uuid id);

    bool triggerEnabled(const nx::Uuid& id) const;
    bool prolongedTrigger(const nx::Uuid& id) const;
    QString triggerName(const nx::Uuid& id) const;
    QString triggerIcon(const nx::Uuid& id) const;

signals:
    void resourceIdChanged();

    void triggerRemoved(const nx::Uuid& id);
    void triggerAdded(
        const nx::Uuid& id,
        const QString& iconPath,
        const QString& name,
        bool prolonged,
        bool enabled);

    void triggerFieldsChanged(const nx::Uuid& id, TriggerFields fields);

private:
    struct Description;
    using DescriptionPtr = QSharedPointer<Description>;
    using DescriptionsHash = QHash<nx::Uuid, DescriptionPtr>;

private:
    DescriptionPtr findTrigger(nx::Uuid id) const;
    void tryRemoveTrigger(const nx::Uuid& id);

    void updateTriggers();
    void updateTriggerByData(nx::Uuid id, const DescriptionPtr& newData);

    void updateTriggerByRule(const nx::vms::event::RulePtr& rule);

    void updateTriggerByVmsRule(const nx::vms::rules::Rule* rule);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
