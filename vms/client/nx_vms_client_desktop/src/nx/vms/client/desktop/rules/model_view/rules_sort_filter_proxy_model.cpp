// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rules_sort_filter_proxy_model.h"

#include <nx/vms/rules/ini.h>

#include "rules_table_model.h"

namespace nx::vms::client::desktop::rules {

bool RulesSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const auto idColumnIndex =
        sourceModel()->index(sourceRow, RulesTableModel::IdColumn, sourceParent);

    const auto isSystem = idColumnIndex.data(RulesTableModel::IsSystemRuleRole).toBool();
    if (isSystem && !vms::rules::ini().showSystemRules)
        return false;

    const auto filterRegExp = filterRegularExpression();
    if (filterRegExp.pattern().isEmpty())
        return true;

    const auto actionColumnIndex =
        sourceModel()->index(sourceRow, RulesTableModel::ActionColumn, sourceParent);
    const auto eventColumnIndex =
        sourceModel()->index(sourceRow, RulesTableModel::EventColumn, sourceParent);

    return actionColumnIndex.data().toString().contains(filterRegExp)
        || eventColumnIndex.data().toString().contains(filterRegExp);
}

} // namespace nx::vms::client::desktop::rules
