#include "software_triggers_watcher.h"

#include <nx/utils/log/log.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <watchers/user_watcher.h>

namespace {

QString extractIconPath(const nx::vms::event::RulePtr& rule)
{
    //qDebug() << ">>>>>>>>>>>>>>>>>>>>>> " << rule->eventParams().description;
    return lit("images/two_way_audio/mic.png");
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
    base_type(parent)
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto eventRuleManager = commonModule->eventRuleManager();

    connect(eventRuleManager, &vms::event::RuleManager::resetRules,
        this, &SoftwareTriggersWatcher::updateTriggers);
    // TODO: add processing //rule added // rule removed etc


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

void SoftwareTriggersWatcher::forcedUpdateEnabledStates()
{
}

bool SoftwareTriggersWatcher::isEnabled(const QnUuid& id) const
{
    const auto it = m_data.find(id);
    if (it != m_data.end())
        return it.value()->enabled;

    NX_EXPECT(false, "Invalid trigger id");
    return false;
}

QString SoftwareTriggersWatcher::icon(const QnUuid& id) const
{
    const auto it = m_data.find(id);
    if (it != m_data.end())
        return it.value()->iconPath;

    NX_EXPECT(false, "Invalid trigger id");
    return QString();
}


void SoftwareTriggersWatcher::clearTriggers()
{
    while(!m_data.isEmpty())
        removeTrigger(m_data.begin().key());
}

void SoftwareTriggersWatcher::removeTrigger(const QnUuid& id)
{
    if (m_data.remove(id))
        emit triggerRemoved(id);
    else
        NX_EXPECT(false, "Wrong trigger id index");
}

void SoftwareTriggersWatcher::updateTriggers()
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto accessManager = commonModule->resourceAccessManager();
    const auto userWatcher = commonModule->instance<QnUserWatcher>();
    const auto currentUser = userWatcher->user();

    if (!accessManager->hasGlobalPermission(currentUser, Qn::GlobalUserInputPermission))
    {
        clearTriggers();
        return;
    }

    DescriptionsHash newData;
    for(const auto& rule: commonModule->eventRuleManager()->rules())
    {
        if (appropriateSoftwareTriggerRule(rule, currentUser, m_resourceId))
            newData.insert(rule->id(), Description::create(rule));
    }

    const auto oldIds = m_data.keys().toSet();
    const auto newIds = newData.keys().toSet();

    using IdsSet = QSet<DescriptionsHash::key_type>;
    const auto removedIds = IdsSet(oldIds).subtract(newIds);
    const auto addedIds = IdsSet(newIds).subtract(oldIds);

    for (const auto& id: removedIds)
        removeTrigger(id);

    for (const auto& id: addedIds)
    {
        const auto data = newData.value(id);
        m_data.insert(id, data);
        emit triggerAdded(id, data->data, data->iconPath, data->enabled);
    }
}

} // namespace mobile
} // namespace client
} // namespace nx

