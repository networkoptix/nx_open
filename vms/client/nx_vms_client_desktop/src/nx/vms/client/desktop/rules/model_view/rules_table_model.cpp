// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rules_table_model.h"

#include <client_core/client_core_module.h>
#include <nx/vms/api/rules/rule.h>
#include <nx/vms/rules/action_builder.h>
#include <nx/vms/rules/action_field.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_field.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/events/generic_event.h>
#include <nx/vms/rules/rule.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_vms_rules_manager.h>

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
    // As users of the class use raw pointers to the fields owned by the actualRule and got by
    // users with eventFields() and actionFields() methods it is required to prolong lifetime of
    // the fields and delete them only after users is notified about changes.
    actualRule.swap(rule);

    update({Qt::DisplayRole, Qt::CheckStateRole, FieldRole});
}

const vms::rules::Rule* RulesTableModel::SimplifiedRule::rule() const
{
    return actualRule.get();
}

void RulesTableModel::SimplifiedRule::setModelIndex(const QPersistentModelIndex& modelIndex)
{
    index = modelIndex;
}

QPersistentModelIndex RulesTableModel::SimplifiedRule::modelIndex() const
{
    return index;
}

void RulesTableModel::SimplifiedRule::update()
{
    update({Qt::CheckStateRole});
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
    update({Qt::DisplayRole, Qt::CheckStateRole, FieldRole});
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
    update({Qt::DisplayRole, Qt::CheckStateRole, FieldRole});
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
    update({Qt::CheckStateRole});
}

bool RulesTableModel::SimplifiedRule::enabled() const
{
    return actualRule->enabled();
}

void RulesTableModel::SimplifiedRule::setEnabled(bool value)
{
    actualRule->setEnabled(value);
    update({Qt::CheckStateRole});
}

QByteArray RulesTableModel::SimplifiedRule::schedule() const
{
    return actualRule->schedule();
}

void RulesTableModel::SimplifiedRule::setSchedule(const QByteArray& schedule)
{
    actualRule->setSchedule(schedule);
    update({Qt::CheckStateRole});
}

void RulesTableModel::SimplifiedRule::update(const QVector<int>& roles)
{
    if (index.isValid())
    {
        auto model = dynamic_cast<const RulesTableModel*>(index.model());
        if (!model)
            return;

        // It is save cast case updateRule() method doesn't invalidate index.
        const_cast<RulesTableModel*>(model)->updateRule(index, roles);
    }
}

RulesTableModel::RulesTableModel(nx::vms::rules::Engine* engine, QObject* parent):
    QAbstractTableModel(parent),
    engine(engine)
{
    initialise();

    connect(engine, &vms::rules::Engine::ruleAddedOrUpdated, this,
        [this](QnUuid ruleId, bool added)
        {
            // If the current user has an intention to remove a rule with such id, it's changes
            // should be ignored.
            if (removedRules.contains(ruleId))
                return;

            if (added)
                addedRules.erase(ruleId);
            else
                modifiedRules.erase(ruleId);

            if (auto simplifiedRule = rule(ruleId).lock())
            {
                simplifiedRule->setRule(this->engine->cloneRule(ruleId));
                return;
            }

            beginInsertRows({}, simplifiedRules.size(), simplifiedRules.size());

            simplifiedRules.emplace_back(
                new SimplifiedRule(this->engine, this->engine->cloneRule(ruleId)));
            simplifiedRules.back()->setModelIndex(index(simplifiedRules.size() - 1, IdColumn));

            endInsertRows();
        });

    connect(engine, &vms::rules::Engine::ruleRemoved, this,
        [this](QnUuid ruleId)
        {
            removedRules.erase(ruleId);

            auto simplifiedRule = rule(ruleId).lock();
            if (!simplifiedRule)
                return;

            const int row = simplifiedRule->modelIndex().row();

            beginRemoveRows({}, row, row);

            simplifiedRules.erase(simplifiedRules.begin() + row);

            endRemoveRows();
        });

    connect(engine, &vms::rules::Engine::rulesReset, this,
        [this]
        {
            rejectChanges();
        });
}

RulesTableModel::~RulesTableModel()
{
}

int RulesTableModel::rowCount(const QModelIndex& /*parent*/) const
{
    return simplifiedRules.size();
}

int RulesTableModel::columnCount(const QModelIndex& /*parent*/) const
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

    beginInsertRows({}, simplifiedRules.size(), simplifiedRules.size());

    addedRules.insert(newRule->id());

    simplifiedRules.emplace_back(new SimplifiedRule(engine, std::move(newRule)));
    simplifiedRules.back()->setModelIndex(index(simplifiedRules.size() - 1, IdColumn));

    endInsertRows();

    return index(simplifiedRules.size() - 1, IdColumn);
}

bool RulesTableModel::removeRule(const QModelIndex& ruleIndex)
{
    if (!isIndexValid(ruleIndex))
        return false;

    const int row = ruleIndex.row();
    const auto ruleId = simplifiedRules[row]->id();

    beginRemoveRows({}, row, row);

    if (addedRules.contains(ruleId))
        addedRules.erase(ruleId);
    else
        removedRules.insert(ruleId);

    simplifiedRules.erase(simplifiedRules.begin() + row);

    endRemoveRows();

    return true;
}

bool RulesTableModel::removeRule(const QnUuid& id)
{
    if (auto simplifiedRule = rule(id).lock())
        return removeRule(simplifiedRule->modelIndex());

    return false;
}

std::weak_ptr<RulesTableModel::SimplifiedRule> RulesTableModel::rule(
    const QModelIndex& ruleIndex) const
{
    if (!isIndexValid(ruleIndex))
        return {};

    return std::weak_ptr<SimplifiedRule>(simplifiedRules[ruleIndex.row()]);
}

std::weak_ptr<RulesTableModel::SimplifiedRule> RulesTableModel::rule(const QnUuid& id) const
{
    auto simplifiedRule = std::find_if(
        simplifiedRules.cbegin(),
        simplifiedRules.cend(),
        [&id](const std::shared_ptr<SimplifiedRule>& r)
        {
            return r->id() == id;
        });

    if (simplifiedRule == simplifiedRules.cend())
        return {};

    return std::weak_ptr<RulesTableModel::SimplifiedRule>(*simplifiedRule);
}

void RulesTableModel::updateRule(const QModelIndex& ruleIndex, const QVector<int>& roles)
{
    if (!isIndexValid(ruleIndex))
        return;

    const int row = ruleIndex.row();
    const auto ruleId = simplifiedRules.at(row)->id();

    if (!addedRules.contains(ruleId))
    {
        if (isRuleModified(simplifiedRules.at(row).get()))
            modifiedRules.insert(ruleId);
        else
            modifiedRules.erase(ruleId);
    }

    auto eventIndex = index(row, EventColumn);
    auto editedIndex = index(row, EditedStateColumn);
    emit dataChanged(eventIndex, editedIndex, roles);
}

void RulesTableModel::applyChanges(std::function<void(const QString&)> errorHandler)
{
    auto connection = messageBusConnection();
    if (!connection)
        return;

    auto rulesManager = connection->getVmsRulesManager(Qn::kSystemAccess);

    std::set<QnUuid> addedAndModifiedRules;
    addedAndModifiedRules.insert(addedRules.cbegin(), addedRules.cend());
    addedAndModifiedRules.insert(modifiedRules.cbegin(), modifiedRules.cend());

    // Send save rule transactions.
    for (const auto& id: addedAndModifiedRules)
    {
        auto simplifiedRule = rule(id).lock();
        if (!simplifiedRule)
            continue;

        auto serializedRule = engine->serialize(simplifiedRule->rule());
        rulesManager->save(
            serializedRule,
            [this, errorHandler, id](int /*requestId*/, ec2::ErrorCode errorCode)
            {
                if (!addedRules.contains(id) && !modifiedRules.contains(id))
                    return;

                if (errorCode == ec2::ErrorCode::ok)
                    return;

                if (errorHandler)
                    errorHandler(ec2::toString(errorCode));
            },
            this
        );
    }

    // Send remove rule transactions.
    for (const auto& id: removedRules)
    {
        rulesManager->deleteRule(
            id,
            [this, id, errorHandler](int /*requestId*/, ec2::ErrorCode errorCode)
            {
                if (!removedRules.contains(id))
                    return;

                if (errorCode == ec2::ErrorCode::ok)
                    return;

                if (errorHandler)
                    errorHandler(ec2::toString(errorCode));
            },
            this
        );
    }
}

void RulesTableModel::rejectChanges()
{
    beginResetModel();

    initialise();

    endResetModel();
}

void RulesTableModel::resetToDefaults(std::function<void(const QString&)> errorHandler)
{
    if (auto connection = messageBusConnection())
    {
        connection->getVmsRulesManager(Qn::kSystemAccess)->resetVmsRules(
            [errorHandler](int /*requestId*/, ec2::ErrorCode errorCode)
            {
                if (errorCode == ec2::ErrorCode::ok)
                    return;

                if (errorHandler)
                    errorHandler(ec2::toString(errorCode));
            },
            this);
    }
}

bool RulesTableModel::hasChanges() const
{
    return !addedRules.empty() || !modifiedRules.empty() || !removedRules.empty();
}

void RulesTableModel::initialise()
{
    auto clonedRules = engine->cloneRules();

    addedRules.clear();
    modifiedRules.clear();
    removedRules.clear();

    simplifiedRules.clear();
    simplifiedRules.reserve(clonedRules.size());

    for (auto& [id, rule]: clonedRules)
    {
        auto eventDescriptor =
            engine->eventDescriptor(rule->eventFilters().constFirst()->eventType());
        auto actionDescriptor =
            engine->actionDescriptor(rule->actionBuilders().constFirst()->actionType());
        if (!eventDescriptor || !actionDescriptor)
            continue;

        simplifiedRules.emplace_back(new SimplifiedRule(engine, std::move(rule)));
        simplifiedRules.back()->setModelIndex(index(simplifiedRules.size() - 1, IdColumn));
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
            return simplifiedRules[index.row()]->id().toString();
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
        return simplifiedRules[index.row()]->eventDescriptor()->displayName;

    return {};
}

QVariant RulesTableModel::actionColumnData(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
            return simplifiedRules[index.row()]->actionDescriptor()->displayName;
    }

    return {};
}

QVariant RulesTableModel::editedStateColumnData(const QModelIndex& index, int role) const
{
    if (role == Qt::CheckStateRole)
        return isRuleModified(simplifiedRules[index.row()].get()) ? Qt::Checked : Qt::Unchecked;

    return {};
}

QVariant RulesTableModel::enabledStateColumnData(const QModelIndex& index, int role) const
{
    if (role == Qt::CheckStateRole)
        return simplifiedRules[index.row()]->enabled() ? Qt::Checked : Qt::Unchecked;

    return {};
}

bool RulesTableModel::setEnabledStateColumnData(
    const QModelIndex& index, const QVariant& value, int role)
{
    if (role == Qt::CheckStateRole)
    {
        simplifiedRules[index.row()]->setEnabled(value.toInt() == Qt::Checked ? true : false);
        return true;
    }

    return false;
}

QVariant RulesTableModel::commentColumnData(const QModelIndex& index, int role) const
{
    if (role == Qt::DisplayRole)
        return simplifiedRules[index.row()]->comment();

    return {};
}

} // namespace nx::vms::client::desktop::rules
