#pragma once

#include <QtCore/QSet>

#include <utils/common/connective.h>

#include <nx/vms/event/rule_manager.h>
#include <nx/mediaserver/metadata/rule_holder.h>
#include <nx/mediaserver/server_module_aware.h>

namespace nx {
namespace mediaserver {
namespace metadata {

class EventRuleWatcher: public Connective<QObject>, public ServerModuleAware
{
    Q_OBJECT
public:
    EventRuleWatcher(QnMediaServerModule* serverModule);
    virtual ~EventRuleWatcher();

    void at_rulesReset(const nx::vms::event::RuleList& rules);
    void at_ruleAddedOrUpdated(const nx::vms::event::RulePtr& rule, bool added);
    void at_ruleRemoved(const QnUuid& ruleId);
    void at_resourceAdded(const QnResourcePtr& resource);

    QSet<QnUuid> watchedEventsForResource(const QnUuid& resourceId);

signals:
    void rulesUpdated(const QSet<QnUuid>& affectedResources);

private:
    RuleHolder m_ruleHolder;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
