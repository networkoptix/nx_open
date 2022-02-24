// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rules_list_model.h"

#include <nx/utils/log/assert.h>

#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/action_builder.h>

#include <nx/vms/client/desktop/vms_rules/vms_rules_action_event_type_display.h>

#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>

namespace nx::vms::client::desktop {
namespace vms_rules {

std::function<entity_item_model::AbstractItemPtr(const QnUuid&)> vmsRuleItemCreator(
    const nx::vms::rules::Engine::RuleSet* ruleSet)
{
    return
        [ruleSet](const QnUuid& ruleId) -> entity_item_model::AbstractItemPtr
        {
            const auto ruleItr = ruleSet->find(ruleId);
            if (ruleItr == std::cend(*ruleSet))
            {
                NX_ASSERT(false);
                return {};
            }
            const auto rulePtr = ruleItr->second.get();

            const auto eventTypeDisplay = getDisplayName(rulePtr->eventFilters().constFirst());
            const auto actionTypeDisplay = getDisplayName(rulePtr->actionBuilders().constFirst());

            QStringList searchStrings{eventTypeDisplay, actionTypeDisplay, rulePtr->comment()};

            return entity_item_model::GenericItemBuilder()
                .withRole(VmsRulesModelRoles::ruleIdRole, QVariant::fromValue<QnUuid>(ruleId))
                .withRole(VmsRulesModelRoles::eventDescriptionTextRole, eventTypeDisplay)
                .withRole(VmsRulesModelRoles::actionDescriptionTextRole, actionTypeDisplay)
                .withRole(VmsRulesModelRoles::ruleModificationMarkRole, false)
                .withRole(VmsRulesModelRoles::ruleEnabledRole, rulePtr->enabled())
                .withRole(VmsRulesModelRoles::commentRole, rulePtr->comment())
                .withRole(VmsRulesModelRoles::searchStringsRole, searchStrings)
                .withFlags({Qt::ItemIsEnabled, Qt::ItemIsSelectable, Qt::ItemNeverHasChildren});
        };
}

} // namespace vms_rules
} // namespace nx::vms::client::desktop
