#include "software_triggers_watcher.h"

#include <nx/utils/log/log.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/global_permissions_manager.h>
#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <nx/client/core/watchers/user_watcher.h>
#include <utils/common/synctime.h>
#include <nx/vms/event/strings_helper.h>

namespace {

QString extractIconPath(const nx::vms::event::RulePtr& rule)
{
    return QStringLiteral("qrc:///images/soft_trigger/user_selectable/%1.png")
        .arg(rule->eventParams().description);
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
    if (rule->isDisabled() || rule->eventType() != nx::vms::api::softwareTriggerEvent)
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
    QString iconPath;
    QString name;
    bool prolonged = false;
    bool enabled = true;

    static SoftwareTriggersWatcher::DescriptionPtr create(const nx::vms::event::RulePtr& rule);
};

SoftwareTriggersWatcher::DescriptionPtr SoftwareTriggersWatcher::Description::create(
    const nx::vms::event::RulePtr& rule)
{
    const auto params = rule->eventParams();
    const auto name = vms::event::StringsHelper::getSoftwareTriggerName(params.caption);
    const bool prolonged = rule->isActionProlonged();
    return DescriptionPtr(new Description({extractIconPath(rule), name, prolonged, true}));
}

//-------------------------------------------------------------------------------------------------

SoftwareTriggersWatcher::SoftwareTriggersWatcher(QObject* parent):
    base_type(parent),
    m_ruleManager(eventRuleManager())
{
    connect(m_ruleManager, &vms::event::RuleManager::rulesReset,
        this, &SoftwareTriggersWatcher::updateTriggers);
    connect(m_ruleManager, &vms::event::RuleManager::ruleAddedOrUpdated,
        this, &SoftwareTriggersWatcher::updateTriggerByRule);
    connect(m_ruleManager, &vms::event::RuleManager::ruleRemoved,
        this, &SoftwareTriggersWatcher::tryRemoveTrigger);

    connect(this, &SoftwareTriggersWatcher::resourceIdChanged,
        this, &SoftwareTriggersWatcher::updateTriggers);

    const auto manager = globalPermissionsManager();
    const auto userWatcher = commonModule()->instance<nx::vms::client::core::UserWatcher>();
    connect(userWatcher, &nx::vms::client::core::UserWatcher::userChanged,
        this, &SoftwareTriggersWatcher::updateTriggers);
    connect(manager, &QnGlobalPermissionsManager::globalPermissionsChanged, this,
        [this, userWatcher]
            (const QnResourceAccessSubject& subject, GlobalPermissions /*permissions*/)
        {
            const auto user = userWatcher->user();
            if (subject != user)
                return;

            updateTriggers();
        });
}

void SoftwareTriggersWatcher::setResourceId(const QnUuid& resourceId)
{
    if (resourceId == m_resourceId)
        return;

    m_resourceId = resourceId;
    emit resourceIdChanged();
}

QString SoftwareTriggersWatcher::triggerName(const QnUuid& id) const
{
    const auto it = m_data.find(id);
    if (it != m_data.end())
        return it.value()->name;

    NX_ASSERT(false, "Invalid trigger id");
    return QString();
}

bool SoftwareTriggersWatcher::prolongedTrigger(const QnUuid& id) const
{
    const auto it = m_data.find(id);
    if (it != m_data.end())
        return it.value()->prolonged;

    NX_ASSERT(false, "Invalid trigger id");
    return false;
}

bool SoftwareTriggersWatcher::triggerEnabled(const QnUuid& id) const
{
    const auto it = m_data.find(id);
    if (it != m_data.end())
        return it.value()->enabled;

    NX_ASSERT(false, "Invalid trigger id");
    return false;
}

QString SoftwareTriggersWatcher::triggerIcon(const QnUuid& id) const
{
    const auto it = m_data.find(id);
    if (it != m_data.end())
        return it.value()->iconPath;

    NX_ASSERT(false, "Invalid trigger id");
    return QString();
}

void SoftwareTriggersWatcher::tryRemoveTrigger(const QnUuid& id)
{
    if (m_data.remove(id))
        emit triggerRemoved(id);
}

void SoftwareTriggersWatcher::updateTriggers()
{
    auto removedIds = m_data.keys().toSet();
    for (const auto& rule: m_ruleManager->rules())
    {
        updateTriggerByRule(rule);
        removedIds.remove(rule->id());
    }

    for (const auto& removedId: removedIds)
        tryRemoveTrigger(removedId);
}

void SoftwareTriggersWatcher::updateTriggersAvailability()
{
    for (const auto& id: m_data.keys())
        updateTriggerAvailability(id, true);
}

void SoftwareTriggersWatcher::updateTriggerByRule(const nx::vms::event::RulePtr& rule)
{
    const auto id = rule->id();
    const auto currentUser = commonModule()->instance<nx::vms::client::core::UserWatcher>()->user();
    if (!globalPermissionsManager()->hasGlobalPermission(currentUser, GlobalPermission::userInput)
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
        emit triggerAdded(id, data->iconPath, data->name, data->prolonged, data->enabled);
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
    TriggerFields fields = NoField;

    if (newData->iconPath != currentData->iconPath)
    {
        currentData->iconPath = newData->iconPath;
        fields |= IconField;
    }

    if (newData->name != currentData->name)
    {
        currentData->name = newData->name;
        fields |= NameField;
    }

    if (newData->prolonged != currentData->prolonged)
    {
        currentData->prolonged = newData->prolonged;
        fields |= ProlongedField;
    }
    if (fields != NoField)
        emit triggerFieldsChanged(id, fields);
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
        emit triggerFieldsChanged(id, EnabledField);
}

} // namespace mobile
} // namespace client
} // namespace nx

