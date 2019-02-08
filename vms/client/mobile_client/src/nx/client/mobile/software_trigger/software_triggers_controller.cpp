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

namespace nx {
namespace client {
namespace mobile {

SoftwareTriggersController::SoftwareTriggersController(QObject* parent):
    base_type(parent),
    m_commonModule(qnClientCoreModule->commonModule()),
    m_userWatcher(m_commonModule->instance<nx::vms::client::core::UserWatcher>()),
    m_accessManager(m_commonModule->resourceAccessManager()),
    m_ruleManager(m_commonModule->eventRuleManager())
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
    {
        NX_ASSERT(false, "Resource is not camera");
    }

    cancelTriggerAction();
    emit resourceIdChanged();
}

QnUuid SoftwareTriggersController::activeTriggerId() const
{
    return m_activeTriggerId;
}

bool SoftwareTriggersController::hasActiveTrigger() const
{
    return !m_activeTriggerId.isNull();
}

bool SoftwareTriggersController::activateTrigger(const QnUuid& id)
{
    if (!m_activeTriggerId.isNull())
    {
        NX_ASSERT(false, "Can't activate trigger while another in progress");
        return false;
    }

    const auto rule = m_ruleManager->rule(id);
    if (!rule)
    {
        NX_ASSERT(false, "Not rule for specified trigger");
        return false;
    }

    const auto state = rule->isActionProlonged()
        ? vms::event::EventState::active
        : vms::event::EventState::undefined;
    return setTriggerState(id, state);
}

bool SoftwareTriggersController::deactivateTrigger()
{
    if (m_activeTriggerId.isNull())
        return false; //< May be when activation failed.

    const auto rule = m_ruleManager->rule(m_activeTriggerId);
    if (!rule || !rule->isActionProlonged())
    {
        NX_ASSERT(rule, "No rule for specified trigger");
        NX_ASSERT(rule->isActionProlonged(), "Action is not prolonged");
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

bool SoftwareTriggersController::setTriggerState(QnUuid id, vms::event::EventState state)
{
    if (m_resourceId.isNull() || id.isNull())
    {
        NX_ASSERT(!m_resourceId.isNull(), "Invalid resource id");
        NX_ASSERT(!id.isNull(), "Invalid trigger id");
        return false;
    }

    const auto currentUser = m_userWatcher->user();
    if (!m_accessManager->hasGlobalPermission(currentUser, GlobalPermission::userInput))
        return false;

    const auto rule = m_ruleManager->rule(id);
    if (!rule)
    {
        NX_ASSERT(rule, "Trigger does not exist");
        return false;
    }

    m_activeTriggerId = state == vms::event::EventState::active ? id : QnUuid();

    const auto callback =
        [this, state, id](bool success, rest::Handle /*handle*/, const QnJsonRestResult& result)
        {
            if (state == nx::vms::event::EventState::inactive)
                emit triggerDeactivated(id);
            else
                emit triggerActivated(id, success && result.error == QnRestResult::NoError);
        };

    const auto connection = m_commonModule->currentServer()->restConnection();
    connection->softwareTriggerCommand(
        m_resourceId, rule->eventParams().inputPortId, state,
        nx::utils::guarded(this, callback), QThread::currentThread());

    return true;
}

} // namespace mobile
} // namespace client
} // namespace nx
