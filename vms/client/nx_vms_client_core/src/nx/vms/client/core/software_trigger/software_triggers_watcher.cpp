// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "software_triggers_watcher.h"

#include <core/resource/security_cam_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/resource/server.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/customizable_icon_field.h>
#include <nx/vms/rules/event_filter_fields/customizable_text_field.h>
#include <nx/vms/rules/event_filter_fields/source_camera_field.h>
#include <nx/vms/rules/event_filter_fields/source_user_field.h>
#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/events/soft_trigger_event.h>
#include <nx/vms/rules/ini.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils/action.h>
#include <nx/vms/rules/utils/field.h>
#include <utils/common/synctime.h>

namespace {

bool appropriateSoftwareTriggerRule(
    const nx::vms::event::RulePtr& rule,
    const QnUserResourcePtr& currentUser,
    const QnUuid& resourceId)
{
    if (rule->isDisabled() || rule->eventType() != nx::vms::api::softwareTriggerEvent)
        return false;

    if (!rule->eventResources().empty() && !rule->eventResources().contains(resourceId))
        return false;

    if (rule->eventParams().metadata.allUsers)
        return true;

    const auto subjects = rule->eventParams().metadata.instigators;
    if (nx::utils::contains(subjects, currentUser->getId()))
        return true;

    const auto userGroups = nx::vms::common::userGroupsWithParents(currentUser);
    for (const auto& groupId: userGroups)
    {
        if (nx::utils::contains(subjects, groupId))
            return true;
    }

    return false;
}

bool appropriateSoftwareTriggerRule(
    const nx::vms::rules::Rule* rule,
    const QnUserResourcePtr& currentUser,
    QnUuid resourceId)
{
    if (!rule->enabled())
        return false;

    // TODO: #amalov support multiple event rule logic.
    const auto filter = rule->eventFilters()[0];
    if (filter->eventType() != nx::vms::rules::SoftTriggerEvent().manifest().id)
        return false;

    const auto cameraField = filter->fieldByName<nx::vms::rules::SourceCameraField>(
        nx::vms::rules::utils::kCameraIdFieldName);
    const auto userField = filter->fieldByName<nx::vms::rules::SourceUserField>("userId");
    if (!NX_ASSERT(cameraField && userField))
        return false;

    if (!cameraField->acceptAll() && !cameraField->ids().contains(resourceId))
        return false;

    if (userField->acceptAll() || userField->ids().contains(currentUser->getId()))
        return true;

    const auto userGroups = nx::vms::common::userGroupsWithParents(currentUser);
    return userGroups.intersects(userField->ids());
}

} // namespace

namespace nx::vms::client::core {

struct SoftwareTriggersWatcher::Description
{
    int version = 0; // Using 0 version for old event engine, and 1 for new one.
    QString icon;
    QString name;
    bool prolonged = false;
    bool enabled = true;

    static SoftwareTriggersWatcher::DescriptionPtr create(const nx::vms::event::RulePtr& rule);
    static SoftwareTriggersWatcher::DescriptionPtr create(
        const nx::vms::rules::Rule* rule,
        const nx::vms::rules::Engine* engine);
};

SoftwareTriggersWatcher::DescriptionPtr SoftwareTriggersWatcher::Description::create(
    const nx::vms::event::RulePtr& rule)
{
    const auto params = rule->eventParams();
    const auto name = vms::event::StringsHelper::getSoftwareTriggerName(params.caption);
    const bool prolonged = rule->isActionProlonged();
    return DescriptionPtr(
        new Description{/*version*/ 0, params.description, name, prolonged, true});
}

SoftwareTriggersWatcher::DescriptionPtr SoftwareTriggersWatcher::Description::create(
    const nx::vms::rules::Rule *rule,
    const nx::vms::rules::Engine* engine)
{
    // TODO: #amalov Support multiple event logic.
    const auto filter = rule->eventFilters()[0];
    const auto nameField =
        filter->fieldByName<nx::vms::rules::CustomizableTextField>("triggerName");
    const auto iconField =
        filter->fieldByName<nx::vms::rules::CustomizableIconField>("triggerIcon");

    if (!NX_ASSERT(nameField && iconField))
        return nullptr;

    return DescriptionPtr(new Description{
        /*version*/ 1,
        iconField->value(),
        nx::vms::event::StringsHelper::getSoftwareTriggerName(nameField->value()),
        nx::vms::rules::isProlonged(engine, rule->actionBuilders()[0]),
        true});
}

//-------------------------------------------------------------------------------------------------

struct SoftwareTriggersWatcher::Private
{
    QnUuid resourceId;
    ServerResourcePtr server;
    DescriptionsHash data;
    AccessController::NotifierHelper accessNotifier;
};

//-------------------------------------------------------------------------------------------------

SoftwareTriggersWatcher::SoftwareTriggersWatcher(
    SystemContext* context,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(context),
    d(new Private{})
{
    auto ruleManager = systemContext()->eventRuleManager();

    connect(ruleManager, &vms::event::RuleManager::rulesReset,
        this, &SoftwareTriggersWatcher::updateTriggers);
    connect(ruleManager, &vms::event::RuleManager::ruleAddedOrUpdated,
        this, &SoftwareTriggersWatcher::updateTriggerByRule);
    connect(ruleManager, &vms::event::RuleManager::ruleRemoved,
        this, &SoftwareTriggersWatcher::tryRemoveTrigger);

    if (nx::vms::rules::ini().fullSupport)
    {
        auto rulesEngine = systemContext()->vmsRulesEngine();

        connect(rulesEngine, &nx::vms::rules::Engine::rulesReset,
            this, &SoftwareTriggersWatcher::updateTriggers);
        connect(rulesEngine, &nx::vms::rules::Engine::ruleAddedOrUpdated, this,
            [this, rulesEngine](QnUuid id)
            {
                updateTriggerByVmsRule(rulesEngine->rule(id).get());
            });
        connect(rulesEngine, &nx::vms::rules::Engine::ruleRemoved,
            this, &SoftwareTriggersWatcher::tryRemoveTrigger);
    }

    connect(this, &SoftwareTriggersWatcher::resourceIdChanged,
        this, &SoftwareTriggersWatcher::updateTriggers);

    connect(systemContext()->resourcePool(), &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources)
        {
            if (d->server && resources.contains(d->server))
                d->server.clear();
        });
}

SoftwareTriggersWatcher::~SoftwareTriggersWatcher()
{
    // Required here for forward declared scoped pointer destruction.
}

void SoftwareTriggersWatcher::setResourceId(const QnUuid& resourceId)
{
    if (resourceId == d->resourceId)
        return;

    d->resourceId = resourceId;
    d->server.clear();
    d->accessNotifier.reset();

    if (const auto camera =
        resourcePool()->getResourceById<QnSecurityCamResource>(d->resourceId))
    {
        d->server = camera->getParentServer().dynamicCast<ServerResource>();
        d->accessNotifier = accessController()->createNotifier(camera);
        d->accessNotifier.callOnPermissionsChanged(this, &SoftwareTriggersWatcher::updateTriggers);
    }

    emit resourceIdChanged();
}

QString SoftwareTriggersWatcher::triggerName(const QnUuid& id) const
{
    if (const auto trigger = findTrigger(id))
        return trigger->name;

    NX_ASSERT(false, "Invalid trigger id");
    return QString();
}

bool SoftwareTriggersWatcher::prolongedTrigger(const QnUuid& id) const
{
    if (const auto trigger = findTrigger(id))
        return trigger->prolonged;

    NX_ASSERT(false, "Invalid trigger id");
    return false;
}

bool SoftwareTriggersWatcher::triggerEnabled(const QnUuid& id) const
{
    if (const auto trigger = findTrigger(id))
        return trigger->enabled;

    NX_ASSERT(false, "Invalid trigger id");
    return false;
}

QString SoftwareTriggersWatcher::triggerIcon(const QnUuid& id) const
{
    if (const auto trigger = findTrigger(id))
        return trigger->icon;

    NX_ASSERT(false, "Invalid trigger id");
    return QString();
}

void SoftwareTriggersWatcher::tryRemoveTrigger(const QnUuid& id)
{
    if (d->data.remove(id))
        emit triggerRemoved(id);
}

void SoftwareTriggersWatcher::updateTriggers()
{
    auto removedIds = nx::utils::toQSet(d->data.keys());

    for (const auto& rule: systemContext()->eventRuleManager()->rules())
    {
        updateTriggerByRule(rule);
        removedIds.remove(rule->id());
    }

    if (nx::vms::rules::ini().fullSupport)
    {
        for (const auto [id, rule]: systemContext()->vmsRulesEngine()->rules())
        {
            updateTriggerByVmsRule(rule.get());
            removedIds.remove(id);
        }
    }

    for (const auto& removedId: removedIds)
        tryRemoveTrigger(removedId);
}

void SoftwareTriggersWatcher::updateTriggersAvailability()
{
    for (const auto& id: d->data.keys())
        updateTriggerAvailability(id);
}

void SoftwareTriggersWatcher::updateTriggerByRule(const nx::vms::event::RulePtr& rule)
{
    const auto currentUser = systemContext()->userWatcher()->user();
    DescriptionPtr newData;

    const auto resource = resourcePool()->getResourceById(d->resourceId);

    if (systemContext()->resourceAccessManager()->hasPermission(
            currentUser,
            resource,
            Qn::Permission::SoftTriggerPermission)
        && appropriateSoftwareTriggerRule(rule, currentUser, d->resourceId))
    {
        newData = Description::create(rule);
    }

    updateTriggerByData(rule->id(), newData);
}

void SoftwareTriggersWatcher::updateTriggerByVmsRule(const nx::vms::rules::Rule* rule)
{
    const auto currentUser = systemContext()->userWatcher()->user();
    DescriptionPtr newData;

    const auto resource = resourcePool()->getResourceById(d->resourceId);

    if (systemContext()->resourceAccessManager()->hasPermission(
            currentUser,
            resource,
            Qn::Permission::SoftTriggerPermission)
        && appropriateSoftwareTriggerRule(rule, currentUser, d->resourceId))
    {
        newData = Description::create(rule, systemContext()->vmsRulesEngine());
    }

    updateTriggerByData(rule->id(), newData);
}

void SoftwareTriggersWatcher::updateTriggerByData(QnUuid id, const DescriptionPtr& newData)
{
    if (!newData)
    {
        tryRemoveTrigger(id);
        return;
    }

    const auto currentData = findTrigger(id);
    if (!currentData)
    {
        d->data.insert(id, newData);
        emit triggerAdded(id, newData->icon, newData->name, newData->prolonged, newData->enabled);
        return;
    }

    TriggerFields fields = NoField;

    if (newData->icon != currentData->icon)
        fields |= IconField;

    if (newData->name != currentData->name)
        fields |= NameField;

    if (newData->prolonged != currentData->prolonged)
        fields |= ProlongedField;

    if (fields != NoField)
    {
        d->data[id] = newData;
        emit triggerFieldsChanged(id, fields);
    }

    updateTriggerAvailability(id);
}

void SoftwareTriggersWatcher::updateTriggerAvailability(QnUuid id)
{
    const auto description = findTrigger(id);
    if (!description)
        return;

    // FIXME: #sivanov System-wide timezone should be used here when it is introduced.
    const QTimeZone tz = NX_ASSERT(d->server)
        ? d->server->timeZone()
        : QTimeZone::LocalTime;

    const auto dateTime = QDateTime::fromMSecsSinceEpoch(qnSyncTime->currentMSecsSinceEpoch(), tz);

    bool enabled = description->enabled;

    if (description->version == 0)
    {
        const auto rule = systemContext()->eventRuleManager()->rule(id);
        if (!rule)
            return;

        enabled = rule->isScheduleMatchTime(dateTime);
    }
    else
    {
        const auto rule = systemContext()->vmsRulesEngine()->rule(id);
        if (!rule)
            return;

        enabled = rule->timeInSchedule(dateTime);
    }

    if (description->enabled == enabled)
        return;

    description->enabled = enabled;
    emit triggerFieldsChanged(id, EnabledField);
}

SoftwareTriggersWatcher::DescriptionPtr SoftwareTriggersWatcher::findTrigger(QnUuid id) const
{
    const auto it = d->data.find(id);
    return (it == d->data.end()) ? DescriptionPtr() : it.value();
}

} // namespace nx::vms::client::core
