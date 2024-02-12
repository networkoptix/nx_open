// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_lookup_field.h"

#include <api/common_message_processor.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/helpers.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_lookup_list_manager.h>

namespace nx::vms::rules {

TextLookupField::TextLookupField(common::SystemContext* context):
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
            if (m_checkType == TextLookupCheckType::inList
                || m_checkType == TextLookupCheckType::notInList)
            {
                if (NX_ASSERT(nx::Uuid::isUuidString(m_value)) && nx::Uuid{m_value} == data.id)
                    m_list.reset();
            }
        });
}

QString TextLookupField::value() const
{
    return m_value;
}

void TextLookupField::setValue(const QString& value)
{
    if (m_value == value)
        return;

    m_list.reset();
    m_value = value;
    emit valueChanged();
}

TextLookupCheckType TextLookupField::checkType() const
{
    return m_checkType;
}

void TextLookupField::setCheckType(TextLookupCheckType type)
{
    if (m_checkType == type)
        return;

    m_list.reset();
    m_checkType = type;
    emit checkTypeChanged();
}

bool TextLookupField::match(const QVariant& eventValue) const
{
    if (!m_list.has_value())
    {
        if (m_checkType == TextLookupCheckType::containsKeywords
            || m_checkType == TextLookupCheckType::doesNotContainKeywords)
        {
            if (!NX_ASSERT(m_value.isEmpty()
                || !nx::Uuid::isUuidString(m_value), "Check type and value aren't compatible"))
            {
                return {};
            }

            m_list = nx::vms::event::splitOnPureKeywords(m_value);
        }
        else
        {
            if (!NX_ASSERT(nx::Uuid::isUuidString(m_value), "Check type and value aren't compatible"))
                return {};

            const auto lookupList = lookupListManager()->lookupList(nx::Uuid{m_value});
            if (!NX_ASSERT(lookupList.objectTypeId.isEmpty(), "Supports only generic lists"))
                return {};

            m_list = QStringList{};
            for (const auto& entry: lookupList.entries)
                m_list->push_back(entry.begin()->second);
        }
    }

    switch (m_checkType)
    {
        case TextLookupCheckType::containsKeywords:
            return nx::vms::event::checkForKeywords(eventValue.toString(), m_list.value());
        case TextLookupCheckType::doesNotContainKeywords:
            return !nx::vms::event::checkForKeywords(eventValue.toString(), m_list.value());
        case TextLookupCheckType::inList:
            return m_list->contains(eventValue.toString());
        case TextLookupCheckType::notInList:
            return !m_list->contains(eventValue.toString());
    }

    return {};
}

} // namespace nx::vms::rules
