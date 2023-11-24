// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_lookup_field.h"

#include <analytics/db/text_search_utils.h>
#include <api/common_message_processor.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_lookup_list_manager.h>

namespace nx::vms::rules {

namespace {

/** Returns whether the list has entries with the given attributes. */
bool checkForListEntries(
    const api::LookupListData& lookupList, const nx::common::metadata::Attributes& attributes)
{
    for (const auto& attribute: attributes)
    {
        const auto& listAttributes = lookupList.attributeNames;
        if (std::find(listAttributes.cbegin(), listAttributes.cend(), attribute.name)
            == listAttributes.cend())
        {
            return false;
        }
    }

    for (const auto& entry: lookupList.entries)
    {
        bool isMatch{true};
        for (const auto& attribute: attributes)
        {
            const auto entryIt = entry.find(attribute.name);
            if (entryIt == entry.end())
            {
                isMatch = false;
                break;
            }

            if (entryIt->second.isEmpty())
                continue; //< Empty entry value matches any event value.

            if (entryIt->second != attribute.value)
            {
                isMatch = false;
                break;
            }
        }

        if (isMatch)
            return true;
    }

    return false;
}

} // namespace

ObjectLookupField::ObjectLookupField(common::SystemContext* context):
    common::SystemContextAware{context}
{
    const auto messageProcessor = systemContext()->messageProcessor();
    if (!messageProcessor)
        return;

    const auto connection = messageProcessor->connection();
    if (!connection)
        return;

    connect(
        connection->lookupListNotificationManager().get(),
        &ec2::AbstractLookupListNotificationManager::addedOrUpdated,
        this,
        [this](const nx::vms::api::LookupListData& data)
        {
            if (m_checkType != ObjectLookupCheckType::hasAttributes)
            {
                if (NX_ASSERT(QnUuid::isUuidString(m_value)) && QnUuid{m_value} == data.id)
                    m_listOrMatcher.reset();
            }
        });
}

QString ObjectLookupField::value() const
{
    return m_value;
}

void ObjectLookupField::setValue(const QString& value)
{
    if (m_value == value)
        return;

    m_listOrMatcher.reset();
    m_value = value;
    emit valueChanged();
}

ObjectLookupCheckType ObjectLookupField::checkType() const
{
    return m_checkType;
}

void ObjectLookupField::setCheckType(ObjectLookupCheckType type)
{
    if (m_checkType == type)
        return;

    m_listOrMatcher.reset();
    m_checkType = type;
    emit checkTypeChanged();
}

bool ObjectLookupField::match(const QVariant& eventValue) const
{
    if (!m_listOrMatcher.has_value())
    {
        if (m_checkType == ObjectLookupCheckType::hasAttributes)
        {
            if (!NX_ASSERT(m_value.isEmpty()
                || !QnUuid::isUuidString(m_value), "Check type and value aren't compatible"))
            {
                return false;
            }

            analytics::db::TextMatcher textMatcher;
            textMatcher.parse(value());

            m_listOrMatcher = textMatcher;
        }
        else
        {
            if (!NX_ASSERT(QnUuid::isUuidString(m_value), "Check type and value aren't compatible"))
                return false;

            m_listOrMatcher = lookupListManager()->lookupList(QnUuid{m_value});
        }
    }

    const auto attributes = eventValue.value<nx::common::metadata::Attributes>();
    if (m_checkType == ObjectLookupCheckType::hasAttributes)
    {
        auto matcher = std::any_cast<analytics::db::TextMatcher&>(m_listOrMatcher);
        return matcher.matchAttributes(attributes);
    }

    const auto& lookupListData = std::any_cast<api::LookupListData&>(m_listOrMatcher);
    const auto hasEntry = checkForListEntries(lookupListData, attributes);
    return m_checkType == ObjectLookupCheckType::inList ? hasEntry : !hasEntry;
}

} // namespace nx::vms::rules
