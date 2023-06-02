// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rules_sort_filter_proxy_model.h"

#include <QtQml/QtQml>

#include <nx/vms/rules/ini.h>

#include "rules_table_model.h"

namespace nx::vms::client::desktop::rules {

RulesSortFilterProxyModel::RulesSortFilterProxyModel(QObject* parent):
    QSortFilterProxyModel{parent},
    m_rulesTableModel{new RulesTableModel{this}}
{
    setSourceModel(m_rulesTableModel);
    setFilterRole(RulesTableModel::FilterRole);
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

bool RulesSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const auto idColumnIndex =
        sourceModel()->index(sourceRow, RulesTableModel::EventColumn, sourceParent);

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

void RulesSortFilterProxyModel::registerQmlType()
{
    qmlRegisterType<rules::RulesSortFilterProxyModel>(
        "nx.vms.client.desktop", 1, 0, "RulesSortFilterProxyModel");
}

} // namespace nx::vms::client::desktop::rules
