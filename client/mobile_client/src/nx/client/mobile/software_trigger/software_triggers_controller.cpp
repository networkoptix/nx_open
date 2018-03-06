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

    cancelTriggerAction();
    emit resourceIdChanged();
}

bool SoftwareTriggersController::activateTrigger(const QnUuid& id)
{
    if (!m_activeTriggerId.isNull())
    {
        NX_EXPECT(false, "Can't activate trigger while another in progress");
        return false;
    }

    const auto rule = m_ruleManager->rule(id);
    if (!rule)
    {
        NX_EXPECT(false, "Not rule for specified trigger");
        return false;
    }

    m_activeTriggerId = id;
    const auto state = rule->isActionProlonged()
        ? vms::event::EventState::active
        : vms::event::EventState::undefined;
    const bool result = setTriggerState(id, state);
    if (!result)
        m_activeTriggerId = QnUuid();

    return result;
}

bool SoftwareTriggersController::deactivateTrigger()
{
    if (m_activeTriggerId.isNull())
        return false; // May be when activation failed.

    const auto rule = m_ruleManager->rule(m_activeTriggerId);
    if (!rule || !rule->isActionProlonged())
    {
        NX_EXPECT(rule, "No rule for specified trigger");
        NX_EXPECT(rule->isActionProlonged(), "Action is not prolonged");
        return false;
    }

    return setTriggerState(m_activeTriggerId, vms::event::EventState::inactive);
}

void SoftwareTriggersController::cancelTriggerAction()
{
    if (m_activeTriggerId.isNull())
        return;

    const auto rule = m_ruleManager->rule(m_activeTriggerId);
    if (rule && rule->isActionProlonged())
        deactivateTrigger();

    const auto currentTriggerId = m_activeTriggerId;
    m_activeTriggerId = QnUuid();
    emit triggerCancelled(currentTriggerId);
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

    const auto callback =
        [this, state](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            if (handle != m_currentHandle)
                return;

            m_currentHandle = rest::Handle();
            if (m_activeTriggerId.isNull())
                return; // Action cancelled.

            const auto currentId = m_activeTriggerId;
            const bool inactiveState = state == nx::vms::event::EventState::inactive;
            if (inactiveState || state == nx::vms::event::EventState::undefined)
                m_activeTriggerId = QnUuid();

            if (inactiveState)
                triggerDeactivated(currentId);
            else
                triggerActivated(currentId, success && result.error == QnRestResult::NoError);

        };

    const auto connection = m_commonModule->currentServer()->restConnection();
    m_currentHandle = connection->softwareTriggerCommand(
        m_resourceId, rule->eventParams().inputPortId, state,
        QnGuardedCallback<decltype(callback)>(this, callback), QThread::currentThread());

    return true;
}

} // namespace mobile
} // namespace client
} // namespace nx
