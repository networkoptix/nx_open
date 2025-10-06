// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rules_sort_filter_proxy_model.h"

#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/search_helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_builder_field.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_field.h>
#include <nx/vms/rules/ini.h>
#include <nx/vms/rules/rule.h>

#include "rules_table_model.h"

namespace nx::vms::client::desktop::rules {

namespace {

/** Returns whether any of the given camera resources match the given pattern. */
bool matches(const QString& pattern, const UuidSet& ids)
{
    if (!Uuid::fromStringSafe(pattern).isNull() && ids.contains(RulesTableModel::kAnyDeviceUuid))
    {
        // Matched if the rule accept any device and the pattern is a Uuid.
        return true;
    }

    const auto resources = appContext()->currentSystemContext()->resourcePool()
        ->getResourcesByIds<QnVirtualCameraResource>(ids);

    return std::any_of(
        resources.cbegin(),
        resources.cend(),
        [&pattern](const QnResourcePtr& resource)
        {
            return resources::search_helper::matches(pattern, resource);
        });
}

bool matches(const QRegularExpression& pattern, const nx::vms::rules::Rule* rule)
{
    const auto eventFilter = rule->eventFilters().first();
    const auto actionBuilder = rule->actionBuilders().first();

    const auto predicate =
        [&pattern](const nx::vms::rules::Field* field)
        {
            return field->match(pattern);
        };

    return std::ranges::any_of(eventFilter->fields().values(), predicate)
        || std::ranges::any_of(actionBuilder->fields().values(), predicate);
}

} // namespace

RulesSortFilterProxyModel::RulesSortFilterProxyModel(QObject* parent):
    QSortFilterProxyModel{parent},
    m_rulesTableModel{new RulesTableModel{this}}
{
    setSourceModel(m_rulesTableModel);
    setDynamicSortFilter(true);
    setSortRole(RulesTableModel::SortDataRole);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setFilterKeyColumn(-1);
    connect(
        appContext()->cloudServiceChecker(),
        &nx::vms::client::core::CloudServiceChecker::supportedServicesChanged,
        this,
        [this](nx::vms::client::core::CloudServiceChecker::CloudServices changedServices)
        {
            if (changedServices.testFlag(nx::vms::client::core::CloudService::push_notifications))
                invalidateFilter();
        });
}

bool RulesSortFilterProxyModel::filterAcceptsRow(
    int sourceRow, const QModelIndex& sourceParent) const
{
    const auto idColumnIndex =
        m_rulesTableModel->index(sourceRow, RulesTableModel::EventColumn, sourceParent);

    const auto isSystem = idColumnIndex.data(RulesTableModel::IsSystemRuleRole).toBool();
    if (isSystem && !vms::rules::ini().showSystemRules)
        return false;

    const auto filterRegExp = filterRegularExpression();
    if (filterRegExp.pattern().isEmpty())
        return true;

    const auto eventColumnIndex =
        m_rulesTableModel->index(sourceRow, RulesTableModel::EventColumn, sourceParent);
    const auto sourceColumnIndex =
        m_rulesTableModel->index(sourceRow, RulesTableModel::SourceColumn, sourceParent);
    const auto actionColumnIndex =
        m_rulesTableModel->index(sourceRow, RulesTableModel::ActionColumn, sourceParent);
    const auto targetColumnIndex =
        m_rulesTableModel->index(sourceRow, RulesTableModel::TargetColumn, sourceParent);
    const auto commentColumnIndex =
        m_rulesTableModel->index(sourceRow, RulesTableModel::CommentColumn, sourceParent);

    return actionColumnIndex.data().toString().contains(filterRegExp)
        || eventColumnIndex.data().toString().contains(filterRegExp)
        || commentColumnIndex.data().toString().contains(filterRegExp)
        || sourceColumnIndex.data().toString().contains(filterRegExp)
        || targetColumnIndex.data().toString().contains(filterRegExp)
        || matches(filterRegExp.pattern(),
            sourceColumnIndex.data(RulesTableModel::ResourceIdsRole).value<UuidSet>())
        || matches(filterRegExp.pattern(),
            targetColumnIndex.data(RulesTableModel::ResourceIdsRole).value<UuidSet>())
        || matches(filterRegExp, m_rulesTableModel->getRule(idColumnIndex).get());
}

UuidList RulesSortFilterProxyModel::getRuleIds(const QList<int>& rows) const
{
    UuidList result;
    for (const auto row: rows)
    {
        const auto ruleId =
            m_rulesTableModel->data(mapToSource(index(row, 0)), RulesTableModel::RuleIdRole);

        if (ruleId.isValid())
            result.push_back(ruleId.value<nx::Uuid>());
    }

    return result;
}

Qt::CheckState RulesSortFilterProxyModel::getRuleCheckStates(const QList<int>& rows) const
{
    if (rows.empty())
        return {};

    const auto getRuleState = [this](int row)
    {
        const auto checkState = m_rulesTableModel->data(
            mapToSource(index(row, RulesTableModel::StateColumn)), Qt::CheckStateRole);
        return checkState.value<Qt::CheckState>() == Qt::CheckState::Checked;
    };

    auto rowsIt = rows.cbegin();
    const auto isEnabled = getRuleState(*rowsIt);
    while (rowsIt != rows.cend())
    {
        if (getRuleState(*rowsIt) != isEnabled)
            return Qt::CheckState::PartiallyChecked;

        ++rowsIt;
    }

    return isEnabled ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;
}

void RulesSortFilterProxyModel::registerQmlType()
{
    qmlRegisterType<rules::RulesSortFilterProxyModel>(
        "nx.vms.client.desktop", 1, 0, "RulesSortFilterProxyModel");
}

} // namespace nx::vms::client::desktop::rules
