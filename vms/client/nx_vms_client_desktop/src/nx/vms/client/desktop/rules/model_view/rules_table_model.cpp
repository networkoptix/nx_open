// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "rules_table_model.h"

#include <nx/vms/api/rules/rule.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_field.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/event_field.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/events/generic_event.h>

namespace nx::vms::client::desktop::rules {

using Engine = vms::rules::Engine;
using Field = vms::rules::Field;
using Rule = vms::rules::Rule;
using ItemDescriptor = vms::rules::ItemDescriptor;

namespace {

bool operator==(const Rule& left, const Rule& right)
{
    if (left.comment() != right.comment())
        return false;

    if (left.enabled() != right.enabled())
        return false;

    if (left.schedule() != right.schedule())
        return false;

    auto lEventFilters = left.eventFilters();
    auto rEventFilters = right.eventFilters();
    if (lEventFilters.size() != rEventFilters.size())
        return false;

    for (int i = 0; i < lEventFilters.size(); ++i)
    {
        if (lEventFilters[i]->eventType() != rEventFilters[i]->eventType())
            return false;

        if (lEventFilters[i]->flatData() != rEventFilters[i]->flatData())
            return false;
    }

    auto lActionBuilders = left.actionBuilders();
    auto rActionBuilders = right.actionBuilders();
    if (lActionBuilders.size() != rActionBuilders.size())
        return false;

    for (int i = 0; i < lActionBuilders.size(); ++i)
    {
        if (lActionBuilders[i]->actionType() != rActionBuilders[i]->actionType())
            return false;

        if (lActionBuilders[i]->flatData() != rActionBuilders[i]->flatData())
            return false;
    }

    return true;
}

} // namespace

RulesTableModel::SimplifiedRule::SimplifiedRule(
    Engine* engine,
    std::unique_ptr<vms::rules::Rule>&& rule)
    :
    engine{engine},
    actualRule{std::move(rule)}
{
    NX_ASSERT(engine);
    NX_ASSERT(actualRule);
}

RulesTableModel::SimplifiedRule::~SimplifiedRule()
{
}

void RulesTableModel::SimplifiedRule::setRule(std::unique_ptr<vms::rules::Rule>&& rule)
{
    actualRule = std::move(rule);
    update();
}

const vms::rules::Rule* RulesTableModel::SimplifiedRule::rule() const
{
    return actualRule.get();
}

void RulesTableModel::SimplifiedRule::setModelIndex(const QPersistentModelIndex& modelIndex)
{
    index = modelIndex;
}

void RulesTableModel::SimplifiedRule::update()
{
    if (index.isValid())
    {
        auto model = dynamic_cast<const RulesTableModel*>(index.model());
        if (!model)
            return;

        // It is save cast case updateRule() method doesn't invalidate index.
        const_cast<RulesTableModel*>(model)->updateRule(index);
    }
}

QnUuid RulesTableModel::SimplifiedRule::id() const
{
    return actualRule->id();
}

QString RulesTableModel::SimplifiedRule::eventType() const
{
    if (!actualRule->eventFilters().empty())
        return actualRule->eventFilters().constFirst()->eventType();

    return "";
}

void RulesTableModel::SimplifiedRule::setEventType(const QString& eventType)
{
    if (!actualRule->eventFilters().empty())
        actualRule->takeEventFilter(0);

    actualRule->addEventFilter(engine->buildEventFilter(eventType));
    update();
}

QHash<QString, Field*> RulesTableModel::SimplifiedRule::eventFields() const
{
    if (actualRule->eventFilters().empty())
        return {};

    auto fields = actualRule->eventFilters().constFirst()->fields();
    return QHash<QString, Field*>{fields.cbegin(), fields.cend()};
}

std::optional<ItemDescriptor> RulesTableModel::SimplifiedRule::eventDescriptor() const
{
    return engine->eventDescriptor(eventType());
}

QString RulesTableModel::SimplifiedRule::actionType() const
{
    if (!actualRule->actionBuilders().empty())
        return actualRule->actionBuilders().constFirst()->actionType();

    return "";
}

void RulesTableModel::SimplifiedRule::setActionType(const QString& actionType)
{
    if (!actualRule->actionBuilders().empty())
        actualRule->takeActionBuilder(0);

    actualRule->addActionBuilder(engine->buildActionBuilder(actionType));
    update();
}

QHash<QString, Field*> RulesTableModel::SimplifiedRule::actionFields() const
{
    if (actualRule->actionBuilders().empty())
        return {};

    auto fields = actualRule->actionBuilders().constFirst()->fields();
    return QHash<QString, Field*>{fields.cbegin(), fields.cend()};
}

std::optional<ItemDescriptor> RulesTableModel::SimplifiedRule::actionDescriptor() const
{
    return engine->actionDescriptor(actionType());
}

QString RulesTableModel::SimplifiedRule::comment() const
{
    return actualRule->comment();
}

void RulesTableModel::SimplifiedRule::setComment(const QString& comment)
{
    actualRule->setComment(comment);
    update();
}

bool RulesTableModel::SimplifiedRule::enabled() const
{
    return actualRule->enabled();
}

void RulesTableModel::SimplifiedRule::setEnabled(bool value)
{
    actualRule->setEnabled(value);
    update();
}

RulesTableModel::RulesTableModel(QObject* parent):
    QAbstractTableModel(parent), engine(Engine::instance())
{
    initialise();
}

RulesTableModel::~RulesTableModel()
{
}

int RulesTableModel::rowCount(const QModelIndex& parent) const
{
    return rules.size();
}

int RulesTableModel::columnCount(const QModelIndex& parent) const
{
    return ColumnsCount;
}

QVariant RulesTableModel::data(const QModelIndex& index, int role) const
{
    if (!isIndexValid(index))
        return {};

    switch (index.column())
    {
        case IdColumn:
            return idColumnData(index, role);
        case EventColumn:
            return eventColumnData(index, role);
        case ActionColumn:
            return actionColumnData(index, role);
        case EditedStateColumn:
            return editedStateColumnData(index, role);
        case EnabledStateColumn:
            return enabledStateColumnData(index, role);
        case CommentColumn:
            return commentColumnData(index, role);
    }

    return {};
}

bool RulesTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!isIndexValid(index))
        return false;

    switch (index.column())
    {
        case EnabledStateColumn:
            return setEnabledStateColumnData(index, value, role);
    }

    return false;
}

Qt::ItemFlags RulesTableModel::flags(const QModelIndex& index) const
{
    auto flags = QAbstractTableModel::flags(index);

    if (index.column() == EnabledStateColumn)
        flags |= Qt::ItemIsEditable;

    return flags;
}

QVariant RulesTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if (section == EventColumn)
            return tr("Event");
        else if (section == ActionColumn)
            return tr("Action");
        else
            return {};
    }

    return QAbstractTableModel::headerData(section, orientation, role);
}

QModelIndex RulesTableModel::addRule()
{
    beginInsertRows({}, rules.size(), rules.size());

    auto newRuleId = QnUuid::createUuid();

    auto newRule = std::make_unique<Rule>(newRuleId);

    // TODO: Choose default type for the event and action.
    auto eventFilter = engine->buildEventFilter(nx::vms::rules::GenericEvent::manifest().id);
    auto actionBuilder =
        engine->buildActionBuilder(nx::vms::rules::NotificationAction::manifest().id);

    if (!eventFilter || !actionBuilder)
        return {};

    newRule->addEventFilter(std::move(eventFilter));
    newRule->addActionBuilder(std::move(actionBuilder));

    rules.push_back(std::shared_ptr<SimplifiedRule>{new SimplifiedRule(engine, std::move(newRule))});
    rules.back()->setModelIndex(index(rules.size() - 1, IdColumn));

    endInsertRows();

    return index(rules.size() - 1, IdColumn);
}

bool RulesTableModel::removeRule(const QModelIndex& ruleIndex)
{
    if (!isIndexValid(ruleIndex))
        return false;

    int row = ruleIndex.row();

    beginRemoveRows({}, row, row);

    removedRules.insert(rules[row]->id());
    rules.erase(rules.begin() + row);

    endRemoveRows();

    return true;
}

std::weak_ptr<RulesTableModel::SimplifiedRule> RulesTableModel::rule(const QModelIndex& ruleIndex)
{
    if (!isIndexValid(ruleIndex))
        return {};

    return std::weak_ptr<SimplifiedRule>(rules[ruleIndex.row()]);
}

void RulesTableModel::updateRule(const QModelIndex& ruleIndex)
{
    if (!isIndexValid(ruleIndex))
        return;

    int row = ruleIndex.row();

    auto eventIndex = index(row, EventColumn);
    auto editedIndex = index(row, EditedStateColumn);
    emit dataChanged(eventIndex, editedIndex, {Qt::DisplayRole, Qt::CheckStateRole});
}

void RulesTableModel::applyChanges()
{
    // Handle added and changed rules.
    for (const auto& simplifiedRule: rules)
    {
        if (isRuleModified(simplifiedRule.get()))
            engine->addRule(engine->serialize(simplifiedRule->rule()));
    }

    // Handle removed rules.
    for (const auto& id: removedRules)
        engine->removeRule(id);

    removedRules.clear();
}

void RulesTableModel::rejectChanges()
{
    beginResetModel();

    initialise();

    endResetModel();
}

void RulesTableModel::initialise()
{
    auto clonedRules = engine->cloneRules();

    removedRules.clear();
    rules.clear();
    rules.reserve(clonedRules.size());

    for (auto& [id, rule]: clonedRules)
    {
        auto eventDescriptor =
            engine->eventDescriptor(rule->eventFilters().constFirst()->eventType());
        auto actionDescriptor =
            engine->actionDescriptor(rule->actionBuilders().constFirst()->actionType());
        if (!eventDescriptor || !actionDescriptor)
            continue;

        rules.emplace_back(new SimplifiedRule(engine, std::move(rule)));
        rules.back()->setModelIndex(index(rules.size() - 1, IdColumn));
    }
}

bool RulesTableModel::isIndexValid(const QModelIndex& index) const
{
    return index.isValid() && index.model()->hasIndex(index.row(), index.column(), index.parent());
}

bool RulesTableModel::isRuleModified(const SimplifiedRule* rule) const
{
    auto& sourceRuleSet = engine->rules();

    if (!sourceRuleSet.contains(rule->id()))
        return true; //< It is a new rule.

    const auto currentRulePtr = rule->rule();
    const auto sourceRulePtr = sourceRuleSet.at(rule->id());

    return *currentRulePtr != *sourceRulePtr;
}

QVariant RulesTableModel::idColumnData(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
            return rules[index.row()]->id().toString();
        case FilterRole:
            return QString("%1 %2")
                .arg(eventColumnData(index, Qt::DisplayRole).toString())
                .arg(actionColumnData(index, Qt::DisplayRole).toString());
    }

    return {};
}

QVariant RulesTableModel::eventColumnData(const QModelIndex& index, int role) const
{
    if (role == Qt::DisplayRole)
        return rules[index.row()]->eventDescriptor()->displayName;

    return {};
}

QVariant RulesTableModel::actionColumnData(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
            return rules[index.row()]->actionDescriptor()->displayName;
    }

    return {};
}

QVariant RulesTableModel::editedStateColumnData(const QModelIndex& index, int role) const
{
    if (role == Qt::CheckStateRole)
        return isRuleModified(rules[index.row()].get()) ? Qt::Checked : Qt::Unchecked;

    return {};
}

QVariant RulesTableModel::enabledStateColumnData(const QModelIndex& index, int role) const
{
    if (role == Qt::CheckStateRole)
        return rules[index.row()]->enabled() ? Qt::Checked : Qt::Unchecked;

    return {};
}

bool RulesTableModel::setEnabledStateColumnData(
    const QModelIndex& index, const QVariant& value, int role)
{
    if (role == Qt::CheckStateRole)
    {
        rules[index.row()]->setEnabled(value.toInt() == Qt::Checked ? true : false);
        return true;
    }

    return false;
}

QVariant RulesTableModel::commentColumnData(const QModelIndex& index, int role) const
{
    if (role == Qt::DisplayRole)
        return rules[index.row()]->comment();

    return {};
}

} // namespace nx::vms::client::desktop::rules
