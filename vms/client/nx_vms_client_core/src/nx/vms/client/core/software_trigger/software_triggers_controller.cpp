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
#include <utils/common/synctime.h>

namespace nx::vms::client::core {

struct SoftwareTriggersController::Private
{
    Private(SoftwareTriggersController* q);

    bool setTriggerState(QnUuid id, vms::event::EventState state);

    SoftwareTriggersController* const q;
    OrderedRequestsHelper orderedRequestsHelper;

    QnUuid resourceId;
    QnUuid activeTriggerRuleId;
};

SoftwareTriggersController::Private::Private(SoftwareTriggersController* q):
    q(q)
{
}

bool SoftwareTriggersController::Private::setTriggerState(
    QnUuid ruleId,
    vms::event::EventState state)
{
    if (resourceId.isNull() || ruleId.isNull())
    {
        NX_ASSERT(!resourceId.isNull(), "Invalid resource id");
        NX_ASSERT(!ruleId.isNull(), "Invalid trigger id");
        return false;
    }

    const auto currentUser = q->systemContext()->userWatcher()->user();
    if (!q->resourceAccessManager()->hasGlobalPermission(currentUser, GlobalPermission::userInput))
        return false;

    const auto rule = q->systemContext()->eventRuleManager()->rule(ruleId);
    if (!rule)
    {
        NX_ASSERT(rule, "Trigger does not exist");
        return false;
    }

    activeTriggerRuleId = state == vms::event::EventState::active ? ruleId : QnUuid();

    const auto callback = nx::utils::guarded(q,
        [this, state, ruleId](
            bool success, rest::Handle /*handle*/, const nx::network::rest::JsonResult& result)
        {
            success = success && result.error == nx::network::rest::Result::NoError;
            if (state == nx::vms::event::EventState::inactive)
                emit q->triggerDeactivated(ruleId, success);
            else
                emit q->triggerActivated(ruleId, success);
        });

    const auto triggerId = rule->eventParams().inputPortId;
    nx::network::rest::Params params;
    params.insert(lit("timestamp"), lit("%1").arg(qnSyncTime->currentMSecsSinceEpoch()));
    params.insert("event_type",
        nx::reflect::toString(nx::vms::api::EventType::softwareTriggerEvent));
    params.insert(lit("inputPortId"), triggerId);
    params.insert(lit("eventResourceId"), resourceId.toString());

    if (state != nx::vms::api::EventState::undefined)
        params.insert("state", nx::reflect::toString(state));

    orderedRequestsHelper.getJsonResult(q->connectedServerApi(),
        "/api/createEvent", params, callback, QThread::currentThread());
    return true;
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

QnUuid SoftwareTriggersController::resourceId() const
{
    return d->resourceId;
}

void SoftwareTriggersController::setResourceId(const QnUuid& id)
{
    if (d->resourceId == id)
        return;

    const auto pool = systemContext()->resourcePool();
    d->resourceId = pool->getResourceById<QnVirtualCameraResource>(id)
        ? id
        : QnUuid();

    if (d->resourceId.isNull())
        NX_ASSERT(false, "Resource is not camera");

    cancelTriggerAction();
    emit resourceIdChanged();
}

QnUuid SoftwareTriggersController::activeTriggerId() const
{
    return d->activeTriggerRuleId;
}

bool SoftwareTriggersController::hasActiveTrigger() const
{
    return !d->activeTriggerRuleId.isNull();
}

bool SoftwareTriggersController::activateTrigger(const QnUuid& ruleId)
{
    if (!d->activeTriggerRuleId.isNull())
    {
        NX_ASSERT(false, "Can't activate trigger while another in progress");
        return false;
    }

    const auto rule = systemContext()->eventRuleManager()->rule(ruleId);
    if (!rule)
    {
        NX_ASSERT(false, "Not rule for specified trigger");
        return false;
    }

    const auto state = rule->isActionProlonged()
        ? vms::event::EventState::active
        : vms::event::EventState::undefined;
    return d->setTriggerState(ruleId, state);
}

bool SoftwareTriggersController::deactivateTrigger()
{
    if (d->activeTriggerRuleId.isNull())
        return false; //< May be when activation failed.

    const auto rule = systemContext()->eventRuleManager()->rule(d->activeTriggerRuleId);
    if (!rule || !rule->isActionProlonged())
    {
        NX_ASSERT(rule, "No rule for specified trigger");
        NX_ASSERT(rule->isActionProlonged(), "Action is not prolonged");
        return false;
    }

    return d->setTriggerState(d->activeTriggerRuleId, vms::event::EventState::inactive);
}

void SoftwareTriggersController::cancelTriggerAction()
{
    if (d->activeTriggerRuleId.isNull())
        return;

    const auto rule = systemContext()->eventRuleManager()->rule(d->activeTriggerRuleId);
    if (rule && rule->isActionProlonged())
        deactivateTrigger();

    const auto currentTriggerId = d->activeTriggerRuleId;
    d->activeTriggerRuleId = QnUuid();
    emit triggerCancelled(currentTriggerId);
}

} // namespace nx::vms::client::core
