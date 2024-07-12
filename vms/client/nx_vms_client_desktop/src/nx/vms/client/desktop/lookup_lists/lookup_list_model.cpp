// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_model.h"

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/event_filter.h>
#include <nx/vms/rules/event_filter_fields/object_lookup_field.h>
#include <nx/vms/rules/event_filter_fields/text_lookup_field.h>
#include <nx/vms/rules/rule.h>

namespace {

void renameColumnName(nx::vms::api::LookupListData& data)
{
    if (!NX_ASSERT(data.attributeNames.size() == 1, "Generic Lists must have one attribute"))
        return;

    for (auto& entry: data.entries)
    {
        const auto value = entry.begin()->second;
        entry.clear();

        const auto columnName = data.attributeNames[0];
        entry[columnName] = value;
    }
}

} // namespace

namespace nx::vms::client::desktop {

LookupListModel::LookupListModel(QObject* parent):
    QObject(parent)
{
}

LookupListModel::LookupListModel(nx::vms::api::LookupListData data, QObject* parent):
    QObject(parent),
    m_data(std::move(data))
{
}

LookupListModel::~LookupListModel()
{
}

QList<QString> LookupListModel::attributeNames() const
{
    QList<QString> result;
    for (const auto& v: m_data.attributeNames)
        result.push_back(v);
    return result;
}

void LookupListModel::setAttributeNames(QList<QString> value)
{
    if (attributeNames() == value)
        return;

    m_data.attributeNames.clear();
    for (const auto& v: value)
        m_data.attributeNames.push_back(v);

    if (isGeneric())
        renameColumnName(m_data);

    emit attributeNamesChanged();
}

bool LookupListModel::isGeneric() const
{
    return m_data.objectTypeId.isEmpty();
}

void LookupListModel::setObjectTypeId(const QString& objectTypeId)
{
    if (objectTypeId == m_data.objectTypeId)
        return;

    const bool previousIsGenericValue = isGeneric();
    m_data.objectTypeId = objectTypeId;
    emit objectTypeIdChanged();
    if (previousIsGenericValue != isGeneric())
        emit isGenericChanged();
}

QString LookupListModel::objectTypeId() const
{
    return m_data.objectTypeId;
}

QString LookupListModel::name() const
{
    return m_data.name;
}

void LookupListModel::setName(const QString& name)
{
    if (name == m_data.name)
        return;

    m_data.name = name;
    emit nameChanged();
}

int LookupListModel::countOfAssociatedVmsRules(SystemContext* systemContext) const
{
    using namespace nx::vms::rules;
    if (!NX_ASSERT(systemContext))
        return 0;

    int countOfRules = 0;
    const auto rules = systemContext->vmsRulesEngine()->rules();
    for (const auto& [_, rule]: rules)
    {
        if (rule == nullptr)
            continue;

        bool ruleIsChecked = false;
        for (const auto& eventFilter: rule->eventFilters())
        {
            if (eventFilter == nullptr)
                continue;

            for (const auto& field: eventFilter->fields())
            {
                const auto textLookupField = dynamic_cast<TextLookupField*>(field);
                const auto objectLookupField = dynamic_cast<ObjectLookupField*>(field);
                const QString fieldValue = textLookupField
                    ? textLookupField->value()
                    : objectLookupField ? objectLookupField->value() : "";

                if (fieldValue == m_data.id.toString())
                {
                    ruleIsChecked = true;
                    countOfRules += 1;
                    break;
                }
            }

            if (ruleIsChecked)
                break;
        }
    }

    return countOfRules;
}

} // namespace nx::vms::client::desktop
