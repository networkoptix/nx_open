#include "software_triggers_controller.h"

#include <QtQml/QtQml>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/resource_access_manager.h>

#include <nx/client/core/watchers/user_watcher.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/client/core/common/utils/ordered_requests_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::core {

struct SoftwareTriggersController::Private
{
    Private(SoftwareTriggersController* q);

    bool setTriggerState(QnUuid id, vms::event::EventState state);

    SoftwareTriggersController* const q;
    QnCommonModule* const commonModule;
    nx::vms::client::core::UserWatcher* const userWatcher;
    QnResourceAccessManager* const accessManager;
    vms::event::RuleManager* const ruleManager;

    QnUuid resourceId;
    QnUuid activeTriggerId;
};

SoftwareTriggersController::Private::Private(SoftwareTriggersController* q):
    q(q),
    commonModule(qnClientCoreModule->commonModule()),
    userWatcher(commonModule->instance<UserWatcher>()),
    accessManager(commonModule->resourceAccessManager()),
    ruleManager(commonModule->eventRuleManager())
{
}

bool SoftwareTriggersController::Private::setTriggerState(QnUuid id, vms::event::EventState state)
{
    if (resourceId.isNull() || id.isNull())
    {
        NX_ASSERT(!resourceId.isNull(), "Invalid resource id");
        NX_ASSERT(!id.isNull(), "Invalid trigger id");
        return false;
    }

    const auto currentUser = userWatcher->user();
    if (!accessManager->hasGlobalPermission(currentUser, GlobalPermission::userInput))
        return false;

    const auto rule = ruleManager->rule(id);
    if (!rule)
    {
        NX_ASSERT(rule, "Trigger does not exist");
        return false;
    }

    activeTriggerId = state == vms::event::EventState::active ? id : QnUuid();

    const auto callback = nx::utils::guarded(q,
        [this, state, id](bool success, rest::Handle /*handle*/, const QnJsonRestResult& result)
        {
            success = success && result.error == QnRestResult::NoError;
            if (state == nx::vms::event::EventState::inactive)
                emit q->triggerDeactivated(id, success);
            else
                emit q->triggerActivated(id, success);
        });

    const auto connection = commonModule->currentServer()->restConnection();
    QnRequestParamList params;
    params.insert(lit("timestamp"), lit("%1").arg(qnSyncTime->currentMSecsSinceEpoch()));
    params.insert(lit("event_type"), QnLexical::serialized(nx::vms::api::EventType::softwareTriggerEvent));
    params.insert(lit("inputPortId"), id);
    params.insert(lit("eventResourceId"), resourceId.toString());

    if (state != nx::vms::api::EventState::undefined)
        params.insert("state", QnLexical::serialized(state));

    connection->getJsonResult("/api/createEvent", params, callback, QThread::currentThread());
    return true;
}


//--------------------------------------------------------------------------------------------------

SoftwareTriggersController::SoftwareTriggersController(QObject* parent):
    base_type(parent),
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

    const auto pool = d->commonModule->resourcePool();
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
    return d->activeTriggerId;
}

bool SoftwareTriggersController::hasActiveTrigger() const
{
    return !d->activeTriggerId.isNull();
}

bool SoftwareTriggersController::activateTrigger(const QnUuid& id)
{
    if (!d->activeTriggerId.isNull())
    {
        NX_ASSERT(false, "Can't activate trigger while another in progress");
        return false;
    }

    const auto rule = d->ruleManager->rule(id);
    if (!rule)
    {
        NX_ASSERT(false, "Not rule for specified trigger");
        return false;
    }

    const auto state = rule->isActionProlonged()
        ? vms::event::EventState::active
        : vms::event::EventState::undefined;
    return d->setTriggerState(id, state);
}

bool SoftwareTriggersController::deactivateTrigger()
{
    if (d->activeTriggerId.isNull())
        return false; //< May be when activation failed.

    const auto rule = d->ruleManager->rule(d->activeTriggerId);
    if (!rule || !rule->isActionProlonged())
    {
        NX_ASSERT(rule, "No rule for specified trigger");
        NX_ASSERT(rule->isActionProlonged(), "Action is not prolonged");
        return false;
    }

    return d->setTriggerState(d->activeTriggerId, vms::event::EventState::inactive);
}

void SoftwareTriggersController::cancelTriggerAction()
{
    if (d->activeTriggerId.isNull())
        return;

    const auto rule = d->ruleManager->rule(d->activeTriggerId);
    if (rule && rule->isActionProlonged())
        deactivateTrigger();

    const auto currentTriggerId = d->activeTriggerId;
    d->activeTriggerId = QnUuid();
    emit triggerCancelled(currentTriggerId);
}

} // namespace nx::vms::client::core
