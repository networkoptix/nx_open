// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_field.h"

#include <QtCore/QVariant>

#include <analytics/common/object_metadata.h>
#include <api/common_message_processor.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/event/helpers.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_lookup_list_manager.h>

namespace nx::vms::rules {

namespace {

const auto kKeywordAttributeName = "keyword";

vms::api::LookupListData makeLookupList(const QString& keywords)
{
    vms::api::LookupListData lookupList;
    lookupList.attributeNames.emplace_back(kKeywordAttributeName);

    for (const auto& keyword: nx::vms::event::splitOnPureKeywords(keywords))
        lookupList.entries.push_back({{kKeywordAttributeName, keyword}});

    if (lookupList.entries.empty()) //< Keywords absence means any value match.
        lookupList.entries.push_back({{kKeywordAttributeName, ""}});

    return lookupList;
}

} // namespace

LookupField::LookupField(common::SystemContext* context):
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
            if (m_lookupList && m_lookupList->id == data.id)
                m_lookupList = data;
        });
}

QString LookupField::value() const
{
    return m_value;
}

void LookupField::setValue(const QString& value)
{
    if (m_value == value)
        return;

    m_lookupList.reset();

    m_value = value;
    emit valueChanged();
}

LookupCheckType LookupField::checkType() const
{
    return m_checkType;
}

void LookupField::setCheckType(LookupCheckType type)
{
    if (m_checkType == type)
        return;

    m_checkType = type;
    emit checkTypeChanged();
}

LookupSource LookupField::source() const
{
    return m_source;
}

void LookupField::setSource(LookupSource source)
{
    if (m_source == source)
        return;

    m_lookupList.reset();

    m_source = source;
    emit sourceChanged();
}

QStringList LookupField::attributes() const
{
    return m_attributes;
}

void LookupField::setAttributes(const QStringList& attributes)
{
    if (m_attributes == attributes)
        return;

    m_attributes = attributes;
    emit attributesChanged();
}

bool LookupField::match(const QVariant& eventValue) const
{
    if (!m_lookupList)
    {
        if (m_source == LookupSource::keywords)
        {
            m_lookupList = makeLookupList(m_value);
        }
        else
        {
            if (!NX_ASSERT(lookupListManager()->isInitialized()))
                return false;

            m_lookupList = lookupListManager()->lookupList(QnUuid{m_value});
        }
    }

    std::map<QString, QString> eventData;
    if (eventValue.canConvert<nx::common::metadata::Attributes>())
    {
        for (const auto& attribute: eventValue.value<nx::common::metadata::Attributes>())
            eventData.insert({attribute.name, attribute.value});
    }
    else
    {
        for (const auto& attributeName: m_lookupList->attributeNames)
            eventData.insert({attributeName, eventValue.toString()});
    }

    return match(eventData);
}

bool LookupField::match(const std::map<QString, QString>& eventData) const
{
    QStringList attributesToCheck = m_attributes.isEmpty()
        ? QStringList{m_lookupList->attributeNames.begin(), m_lookupList->attributeNames.end()}
        : m_attributes;

    for (const auto& entryAttributes: m_lookupList->entries)
    {
        bool isMatch{true};
        for (const auto& attribute: attributesToCheck)
        {
            const auto entryDataIt = entryAttributes.find(attribute);
            if (entryDataIt == entryAttributes.end())
                continue;

            if (entryDataIt->second.isEmpty()) //< Empty entry value matches any event value.
                continue;

            const auto eventDataIt = eventData.find(attribute);
            if (eventDataIt == eventData.end() || !match(eventDataIt->second, entryDataIt->second))
            {
                isMatch = false;
                break;
            }
        }

        if (isMatch)
            return m_checkType == LookupCheckType::in;
    }

    return m_checkType == LookupCheckType::out;
}

bool LookupField::match(const QString& eventValue, const QString& entryValue) const
{
    return m_source == LookupSource::keywords
        ? eventValue.contains(entryValue)
        : eventValue == entryValue;
}

} // namespace nx::vms::rules
