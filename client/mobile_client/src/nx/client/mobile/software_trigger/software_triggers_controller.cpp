#include "software_triggers_controller.h"

#include <watchers/user_watcher.h>
#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/resource_access_manager.h>
#include <utils/common/guarded_callback.h>
#include <api/server_rest_connection.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>

namespace nx {
namespace client {
namespace mobile {

SoftwareTriggersController::SoftwareTriggersController(QObject* parent):
    base_type(parent),
    m_commonModule(qnClientCoreModule->commonModule()),
    m_userWatcher(m_commonModule->instance<QnUserWatcher>()),
    m_accessManager(m_commonModule->resourceAccessManager()),
    m_ruleManager(m_commonModule->eventRuleManager())
{
}

QString SoftwareTriggersController::resourceId() const
{
    return m_resourceId.toString();
}

void SoftwareTriggersController::setResourceId(const QString& id)
{
    const auto newId = QnUuid::fromStringSafe(id);
    if (m_resourceId == newId)
        return;

    const auto pool = m_commonModule->resourcePool();
    m_resourceId = pool->getResourceById<QnVirtualCameraResource>(newId)
        ? newId
        : QnUuid();

    if (m_resourceId.isNull())
        NX_EXPECT(false, "Resource is not camera");

    cancelAllTriggers();
    emit resourceIdChanged();
}

bool SoftwareTriggersController::activateTrigger(const QnUuid& id)
{
    const auto rule = m_ruleManager->rule(id);
    const auto state = rule->isActionProlonged()
        ? vms::event::EventState::active
        : vms::event::EventState::undefined;
    return setTriggerState(id, state);
}

bool SoftwareTriggersController::deactivateTrigger(const QnUuid& id)
{
    const auto rule = m_ruleManager->rule(id);
    if (!rule->isActionProlonged())
    {
        NX_EXPECT(false, "Action is not prolonged");
        return false;
    }

    return setTriggerState(id, vms::event::EventState::inactive);
}

void SoftwareTriggersController::cancelTriggerAction(const QnUuid& id)
{
    if (m_idToHandle.remove(id))
        emit triggerDeactivated(id);
}

bool SoftwareTriggersController::setTriggerState(const QnUuid& id, vms::event::EventState state)
{
    if (m_resourceId.isNull() || id.isNull())
    {
        NX_EXPECT(!m_resourceId.isNull(), "Invalid resource id");
        NX_EXPECT(!id.isNull(), "Invalid trigger id");
        return false;
    }

    const auto currentUser = m_userWatcher->user();
    if (!m_accessManager->hasGlobalPermission(currentUser, Qn::GlobalUserInputPermission))
        return false;

    const auto rule = m_ruleManager->rule(id);
    if (!rule)
    {
        NX_EXPECT(rule, "Trigger does not exist");
        return false;
    }

    if (m_idToHandle.contains(id))
    {
        NX_EXPECT(false, "Trigger action is in progress");
        return false;
    }

    const auto callback =
         [this, state](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            const auto it = m_handleToId.find(handle);
            if (it == m_handleToId.end())
                return;

            const auto id = it.value();
            m_handleToId.erase(it);
            m_idToHandle.remove(id);

            if (state == vms::event::EventState::inactive)
                emit triggerDeactivated(id);
            else
                emit triggerActivated(id, success && result.error == QnRestResult::NoError);
        };

    const auto connection = m_commonModule->currentServer()->restConnection();
    const auto handle = connection->softwareTriggerCommand(
        m_resourceId, rule->eventParams().inputPortId, state,
        QnGuardedCallback<decltype(callback)>(this, callback), QThread::currentThread());

    m_handleToId.insert(handle, id);
    m_idToHandle.insert(id, handle);
    return true;
}

void SoftwareTriggersController::cancelAllTriggers()
{
    for (const auto id: m_idToHandle.keys())
        cancelTriggerAction(id);
}

} // namespace mobile
} // namespace client
} // namespace nx
