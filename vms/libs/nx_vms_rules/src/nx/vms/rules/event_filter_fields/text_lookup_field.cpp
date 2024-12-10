// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_lookup_field.h"

#include <api/common_message_processor.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/helpers.h>
#include <nx/vms/rules/utils/openapi_doc.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_lookup_list_manager.h>

namespace nx::vms::rules {

TextLookupField::TextLookupField(
    common::SystemContext* context,
    const FieldDescriptor* descriptor)
    :
    EventFilterField{descriptor},
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
            return !m_list->empty()
                && nx::vms::event::checkForKeywords(eventValue.toString(), m_list.value());
        case TextLookupCheckType::notInList:
            return m_list->empty()
                || !nx::vms::event::checkForKeywords(eventValue.toString(), m_list.value());
    }

    return {};
}

QJsonObject TextLookupField::openApiDescriptor(const QVariantMap&)
{
    auto result = utils::constructOpenApiDescriptor<TextLookupField>();
    result[utils::kDescriptionProperty] =
        "This field filters the event identifier by matching a <b>filter expression</b> "
        "(<code>checkType</code>: <b>containsKeywords</b> or <b>doesNotContainKeywords</b>) "
        "or by referencing a <b>lookup list</b> "
        "identified by its ID (<code>checkType</code>: <b>inList</b> or <b>notInList</b>).<br/><br/>"

        "Set the <b>UUID</b> of the Lookup List as the <code>value</code> "
        "for the <code>checkType</code> <b>inList</b> "
        "or <b>notInList</b>. To find the required Lookup List, "
        "refer to the response of the request to <code>/rest/v4/lookuplists.</code> <br/><br/>"

        "To construct a <b>filter expression</b>, enter the required keywords as the "
        "<code>value</code> for the <code>checkType</code> <b>containsKeywords</b> "
        "or <b>doesNotContainKeywords</b>. <br/>"
        "To use multiple keywords in filter expression, use a space as the separator. <br/>"
        "<b>For example:</b> <code> 'keyword1 keyword2'</code><br/>"
        "You can also define a complex keyword phrase with spaces by enclosing it in additional quotes. "
        "<b>For example:</b>  <code>'\\\\\\\"complex keyword with spaces\\\\\\\"'</code>.";
    return result;
}

} // namespace nx::vms::rules
