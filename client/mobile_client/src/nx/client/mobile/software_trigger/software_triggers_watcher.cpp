#include "software_triggers_watcher.h"

#include <nx/utils/log/log.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <watchers/user_watcher.h>
#include <utils/common/synctime.h>
#include <utils/common/guarded_callback.h>
#include <api/server_rest_connection.h>

namespace {

QString extractIconPath(const nx::vms::event::RulePtr& rule)
{
    return lit("images/soft_trigger/user_selectable/%1.png").arg(rule->eventParams().description);
}

template<class Container, class Item>
bool contains(const Container& cont, const Item& item)
{
    return std::find(cont.cbegin(), cont.cend(), item) != cont.cend();
}

bool appropriateSoftwareTriggerRule(
    const nx::vms::event::RulePtr& rule,
    const QnUserResourcePtr& currentUser,
    const QnUuid& resourceId)
{
    if (rule->isDisabled() || rule->eventType() != nx::vms::event::softwareTriggerEvent)
        return false;

    if (!rule->eventResources().empty() && !rule->eventResources().contains(resourceId))
        return false;

    if (rule->eventParams().metadata.allUsers)
        return true;

    const auto subjects = rule->eventParams().metadata.instigators;
    if (contains(subjects, currentUser->getId()))
        return true;

    return contains(subjects, QnUserRolesManager::unifiedUserRoleId(currentUser));
}

} // namespace

namespace nx {
namespace client {
namespace mobile {

struct SoftwareTriggersWatcher::Description
{
    SoftwareTriggerData data;
    QString iconPath;
    bool enabled = true;

    static SoftwareTriggersWatcher::DescriptionPtr create(const nx::vms::event::RulePtr& rule);
};

SoftwareTriggersWatcher::DescriptionPtr SoftwareTriggersWatcher::Description::create(
    const nx::vms::event::RulePtr& rule)
{
    const auto params = rule->eventParams();
    const SoftwareTriggerData data({params.inputPortId, params.caption, rule->isActionProlonged()});
    return DescriptionPtr(new Description({data, extractIconPath(rule), true} /*TODO: add availability check*/));
}

//

SoftwareTriggersWatcher::SoftwareTriggersWatcher(QObject* parent):
    base_type(parent),
    m_commonModule(qnClientCoreModule->commonModule()),
    m_ruleManager(m_commonModule->eventRuleManager())
{
    connect(m_ruleManager, &vms::event::RuleManager::rulesReset,
        this, &SoftwareTriggersWatcher::updateTriggers);
    connect(m_ruleManager, &vms::event::RuleManager::ruleAddedOrUpdated,
        this, &SoftwareTriggersWatcher::updateTriggerByRule);
    connect(m_ruleManager, &vms::event::RuleManager::ruleRemoved,
        this, &SoftwareTriggersWatcher::tryRemoveTrigger);

    connect(this, &SoftwareTriggersWatcher::resourceIdChanged,
        this, &SoftwareTriggersWatcher::updateTriggers);
}

void SoftwareTriggersWatcher::setResourceId(const QnUuid& resourceId)
{
    if (resourceId == m_resourceId)
        return;

    m_resourceId = resourceId;
    emit resourceIdChanged();
}

SoftwareTriggerData SoftwareTriggersWatcher::triggerData(const QnUuid& id) const
{
    const auto it = m_data.find(id);
    if (it != m_data.end())
        return it.value()->data;

    NX_EXPECT(false, "Invalid trigger id");
    return SoftwareTriggerData();
}

bool SoftwareTriggersWatcher::triggerEnabled(const QnUuid& id) const
{
    const auto it = m_data.find(id);
    if (it != m_data.end())
        return it.value()->enabled;

    NX_EXPECT(false, "Invalid trigger id");
    return false;
}

QString SoftwareTriggersWatcher::triggerIcon(const QnUuid& id) const
{
    const auto it = m_data.find(id);
    if (it != m_data.end())
        return it.value()->iconPath;

    NX_EXPECT(false, "Invalid trigger id");
    return QString();
}

void SoftwareTriggersWatcher::tryRemoveTrigger(const QnUuid& id)
{
    if (m_data.remove(id))
        emit triggerRemoved(id);
}

void SoftwareTriggersWatcher::updateTriggers()
{
    for(const auto& rule: m_ruleManager->rules())
        updateTriggerByRule(rule);
}

void SoftwareTriggersWatcher::updateTriggersAvailability()
{
    for (const auto& id: m_data.keys())
        updateTriggerAvailability(id, true);
}

void SoftwareTriggersWatcher::updateTriggerByRule(const nx::vms::event::RulePtr& rule)
{
    const auto id = rule->id();
    const auto accessManager = m_commonModule->resourceAccessManager();
    const auto currentUser = m_commonModule->instance<QnUserWatcher>()->user();
    if (!accessManager->hasGlobalPermission(currentUser, Qn::GlobalUserInputPermission)
        || !appropriateSoftwareTriggerRule(rule, currentUser, m_resourceId))
    {
        tryRemoveTrigger(id);
        return;
    }

    const auto it = m_data.find(rule->id());
    const bool createNewTrigger = it == m_data.end();
    const auto data = createNewTrigger
        ? m_data.insert(id, Description::create(rule)).value()
        : it.value();

    updateTriggerAvailability(id, !createNewTrigger);
    updateTriggerData(id, !createNewTrigger);
    if (createNewTrigger)
        emit triggerAdded(id, data->data, data->iconPath, data->enabled);
}

void SoftwareTriggersWatcher::updateTriggerData(const QnUuid& id, bool emiSignal)
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto eventRuleManager = commonModule->eventRuleManager();

    const auto it = m_data.find(id);
    if (it == m_data.end())
        return;

    const auto& currentData = it.value();
    const auto newData = Description::create(eventRuleManager->rule(id));
    if (newData->iconPath != currentData->iconPath)
    {
        currentData->iconPath = newData->iconPath;
        emit triggerIconChanged(id);
    }

    if (newData->data.name != currentData->data.name)
    {
        currentData->data.name = newData->data.name;
        emit triggerNameChanged(id);
    }

    if (newData->data.triggerId != currentData->data.triggerId)
    {
        currentData->data.triggerId = newData->data.triggerId;
        emit triggerIdChanged(id);
    }

    if (newData->data.prolonged != currentData->data.prolonged)
    {
        currentData->data.prolonged = newData->data.prolonged;
        emit triggerProlongedChanged(id);
    }
}

void SoftwareTriggersWatcher::updateTriggerAvailability(const QnUuid& id, bool emitSignal)
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto rule = commonModule->eventRuleManager()->rule(id);
    if (!rule)
        return;

    const auto descriptionIt = m_data.find(id);
    if (descriptionIt == m_data.end())
        return;

    const bool enabled = rule->isScheduleMatchTime(qnSyncTime->currentDateTime());
    const auto description = descriptionIt.value();
    if (description->enabled == enabled)
        return;

    description->enabled = enabled;
    if (emitSignal)
        emit triggerEnabledChanged(id);
}

bool SoftwareTriggersWatcher::activateTrigger(QnUuid& id)
{
    if (m_resourceId.isNull())
        return false;

    const auto accessManager = m_commonModule->resourceAccessManager();
    const auto currentUser = m_commonModule->instance<QnUserWatcher>()->user();
    if (!accessManager->hasGlobalPermission(currentUser, Qn::GlobalUserInputPermission))
        return false;

    if (m_idToHandle.contains(id))
        return false;

    const auto itData = m_data.find(id);
    if (itData == m_data.end())
        return false;

    const auto callback =
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            const auto it = m_handleToId.find(handle);
            if (it == m_handleToId.end())
                return;

            const auto id = it.value();
            m_handleToId.erase(it);
            m_idToHandle.remove(id);

            success = success && result.error == QnRestResult::NoError;
            emit triggerExecuted(id, success);
        };

    const auto triggerId = itData.value()->data.triggerId;
    const auto connection = m_commonModule->currentServer()->restConnection();
    const auto handle = connection->softwareTriggerCommand(
        m_resourceId, triggerId, nx::vms::event::EventState::active,
        QnGuardedCallback<decltype(callback)>(this, callback), QThread::currentThread());

    m_handleToId.insert(handle, id);
    m_idToHandle.insert(id, handle);
    return true;
}

bool SoftwareTriggersWatcher::cancelTriggerAction(const QnUuid& id)
{
    if (!m_idToHandle.remove(id))
        return false;

    emit triggerExecuted(id, false);
    return true;
}

} // namespace mobile
} // namespace client
} // namespace nx

