// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <nx/vms/rules/engine.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>

namespace nx::vms::client::desktop {
namespace vms_rules {

enum VmsRulesModelRoles
{
    ruleIdRole = Qt::UserRole + 1,
    eventDescriptionTextRole,
    eventParameterIconRole,
    eventParameterTextRole,
    actionDescriptionTextRole,
    actionParameterIconRole,
    actionParameterTextRole,
    ruleModificationMarkRole,
    ruleEnabledRole,
    searchStringsRole,
    commentRole,
};

std::function<entity_item_model::AbstractItemPtr(const QnUuid&)> vmsRuleItemCreator(
    const nx::vms::rules::Engine::RuleSet* ruleSet);

} // namespace vms_rules
} // namespace nx::vms::client::desktop
