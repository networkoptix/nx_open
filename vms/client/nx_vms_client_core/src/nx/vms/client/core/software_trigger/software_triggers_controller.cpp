// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "software_triggers_controller.h"

#include <QtQml/QtQml>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/common/utils/ordered_requests_helper.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter_fields/customizable_icon_field.h>
#include <nx/vms/rules/event_filter_fields/customizable_text_field.h>
#include <nx/vms/rules/event_filter_fields/unique_id_field.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/events/soft_trigger_event.h>
#include <nx/vms/rules/rule.h>
#include <nx/vms/rules/utils.h>
#include <nx/vms/rules/utils/action.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::core {

namespace {

struct TriggerInfo
{
    int version = 0; // Using 0 version for old event engine, and 1 for new one.
    nx::Uuid ruleId;
    bool prolonged = false;

    bool isValid() const { return !ruleId.isNull(); }
};

} // namespace

struct SoftwareTriggersController::Private
{
    Private(SoftwareTriggersController* q);

    bool setTriggerState(const TriggerInfo& trigger, vms::event::EventState state);
    bool setEventTriggerState(const TriggerInfo& trigger, vms::event::EventState state);
    bool setVmsTriggerState(const TriggerInfo& trigger, vms::event::EventState state);

    TriggerInfo triggerInfo(nx::Uuid ruleId) const;
    void setActiveTrigger(nx::Uuid ruleId, vms::event::EventState state, bool success);

    SoftwareTriggersController* const q;
    OrderedRequestsHelper orderedRequestsHelper;

    nx::Uuid resourceId;
    nx::Uuid activeTriggerRuleId;
};

SoftwareTriggersController::Private::Private(SoftwareTriggersController* q):
    q(q)
{
}

bool SoftwareTriggersController::Private::setTriggerState(
    const TriggerInfo& trigger,
    vms::event::EventState state)
{
    if (resourceId.isNull() || trigger.ruleId.isNull())
    {
        NX_ASSERT(!resourceId.isNull(), "Invalid resource id");
        NX_ASSERT(!trigger.ruleId.isNull(), "Invalid trigger id");
        return false;
    }

    const auto resource = q->resourcePool()->getResourceById(resourceId);
    const auto currentUser = q->systemContext()->userWatcher()->user();
    if (!q->resourceAccessManager()->hasPermission(
        currentUser,
        resource,
        Qn::Permission::SoftTriggerPermission))
    {
        return false;
    }

    bool result = (trigger.version > 0)
        ? setVmsTriggerState(trigger, state)
        : setEventTriggerState(trigger, state);

    if (result)
    {
        activeTriggerRuleId = (state == vms::event::EventState::active)
            ? trigger.ruleId
            : nx::Uuid();
    }

    return result;
}

bool SoftwareTriggersController::Private::setEventTriggerState(
    const TriggerInfo& trigger,
    vms::event::EventState state)
{
    const auto rule = q->systemContext()->eventRuleManager()->rule(trigger.ruleId);
    if (!rule)
    {
        NX_ASSERT(rule, "Trigger does not exist");
        return false;
    }

    const auto callback = nx::utils::guarded(q,
        [this, state, ruleId = trigger.ruleId](
            bool success, rest::Handle /*handle*/, const nx::network::rest::JsonResult& result)
        {
            success = success && result.error == nx::network::rest::Result::NoError;
            setActiveTrigger(ruleId, state, success);
        });

    const auto eventParams = rule->eventParams();
    nx::network::rest::Params params;
    params.insert("timestamp", QString::number(qnSyncTime->currentMSecsSinceEpoch()));
    params.insert("event_type",
        nx::reflect::toString(nx::vms::api::EventType::softwareTriggerEvent));
    params.insert("inputPortId", eventParams.inputPortId);
    params.insert("eventResourceId", resourceId.toString());
    params.insert("caption", eventParams.getTriggerName());
    params.insert("description", eventParams.getTriggerIcon());

    if (state != nx::vms::api::EventState::undefined)
        params.insert("state", nx::reflect::toString(state));

    return orderedRequestsHelper.postJsonResult(q->connectedServerApi(), "/api/createEvent",
        params, callback, QThread::currentThread());
}

bool SoftwareTriggersController::Private::setVmsTriggerState(
    const TriggerInfo& trigger,
    vms::event::EventState state)
{
    const auto rule = q->systemContext()->vmsRulesEngine()->rule(trigger.ruleId);
    if (!NX_ASSERT(rule, "Trigger does not exist"))
        return false;

    const auto filter = rule->eventFilters()[0];
    const auto idField = filter->fieldByName<nx::vms::rules::UniqueIdField>("triggerId");
    const auto nameField =
        filter->fieldByName<nx::vms::rules::CustomizableTextField>("triggerName");
    const auto iconField =
        filter->fieldByName<nx::vms::rules::CustomizableIconField>("triggerIcon");
    if (!NX_ASSERT(idField && nameField && iconField, "Invalid event manifest"))
        return false;

    auto triggerEvent = QSharedPointer<nx::vms::rules::SoftTriggerEvent>::create(
        qnSyncTime->currentTimePoint(),
        nx::vms::rules::convertEventState(state),
        idField->id(),
        resourceId,
        q->systemContext()->userWatcher()->user()->getId(),
        nx::vms::event::StringsHelper::getSoftwareTriggerName(nameField->value()),
        iconField->value());

    q->systemContext()->vmsRulesEngine()->processEvent(triggerEvent);

    executeLater(
        [this, trigger, state]() {setActiveTrigger(trigger.ruleId, state, /*success*/ true); },
        q);

    return true;
}

void SoftwareTriggersController::Private::setActiveTrigger(
    nx::Uuid ruleId,
    vms::event::EventState state,
    bool success)
{
    if (state == nx::vms::event::EventState::inactive)
        emit q->triggerDeactivated(ruleId, success);
    else
        emit q->triggerActivated(ruleId, success);
}

TriggerInfo SoftwareTriggersController::Private::triggerInfo(nx::Uuid ruleId) const
{
    if (const auto rule = q->systemContext()->eventRuleManager()->rule(ruleId))
        return TriggerInfo{/*version*/ 0, ruleId, rule->isActionProlonged()};

    if (const auto rule = q->systemContext()->vmsRulesEngine()->rule(ruleId))
    {
        const auto firstActionBuilder = rule->actionBuilders()[0];

        return TriggerInfo{
            /*version*/ 1,
            ruleId,
            nx::vms::rules::isProlonged(q->systemContext()->vmsRulesEngine(), firstActionBuilder)};
    }

    return {};
}

//--------------------------------------------------------------------------------------------------

SoftwareTriggersController::SoftwareTriggersController(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext),
    d(new Private(this))
{
}

SoftwareTriggersController::SoftwareTriggersController():
    QObject(),
    SystemContextAware(SystemContext::fromQmlContext(this)),
    d(new Private(this))
{
}

SoftwareTriggersController::~SoftwareTriggersController()
{
    cancelTriggerAction();
}

void SoftwareTriggersController::registerQmlType()
{
    qmlRegisterType<SoftwareTriggersController>("nx.client.mobile", 1, 0, "SoftwareTriggersController");
}

nx::Uuid SoftwareTriggersController::resourceId() const
{
    return d->resourceId;
}

void SoftwareTriggersController::setResourceId(const nx::Uuid& id)
{
    if (d->resourceId == id)
        return;

    const auto pool = systemContext()->resourcePool();
    d->resourceId = pool->getResourceById<QnVirtualCameraResource>(id)
        ? id
        : nx::Uuid();

    if (d->resourceId.isNull())
        NX_ASSERT(false, "Resource is not camera");

    cancelTriggerAction();
    emit resourceIdChanged();
}

nx::Uuid SoftwareTriggersController::activeTriggerId() const
{
    return d->activeTriggerRuleId;
}

bool SoftwareTriggersController::hasActiveTrigger() const
{
    return !d->activeTriggerRuleId.isNull();
}

bool SoftwareTriggersController::activateTrigger(const nx::Uuid& ruleId)
{
    if (!d->activeTriggerRuleId.isNull())
    {
        NX_ASSERT(false, "Can't activate trigger while another in progress");
        return false;
    }

    const auto trigger = d->triggerInfo(ruleId);
    if (!trigger.isValid())
    {
        NX_ASSERT(false, "Not rule for specified trigger");
        return false;
    }

    const auto state = trigger.prolonged
        ? vms::event::EventState::active
        : vms::event::EventState::undefined;
    return d->setTriggerState(trigger, state);
}

bool SoftwareTriggersController::deactivateTrigger()
{
    if (d->activeTriggerRuleId.isNull())
        return false; //< May be when activation failed.

    const auto trigger = d->triggerInfo(d->activeTriggerRuleId);
    if (!trigger.isValid() || !trigger.prolonged)
    {
        NX_ASSERT(trigger.isValid(), "No rule for specified trigger");
        NX_ASSERT(trigger.prolonged, "Action is not prolonged");
        return false;
    }

    return d->setTriggerState(trigger, vms::event::EventState::inactive);
}

void SoftwareTriggersController::cancelTriggerAction()
{
    if (d->activeTriggerRuleId.isNull())
        return;

    const auto trigger = d->triggerInfo(d->activeTriggerRuleId);
    if (trigger.isValid() && trigger.prolonged)
        deactivateTrigger();

    const auto currentTriggerId = d->activeTriggerRuleId;
    d->activeTriggerRuleId = nx::Uuid();
    emit triggerCancelled(currentTriggerId);
}

} // namespace nx::vms::client::core
