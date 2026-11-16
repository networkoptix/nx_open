// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_lookup_field.h"

#include <cstdint>
#include <optional>

#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/event/helpers.h>
#include <nx/vms/rules/utils/openapi_doc.h>

namespace nx::vms::rules {

namespace {

bool isKeywordCheck(TextLookupCheckType checkType)
{
    return checkType == TextLookupCheckType::containsKeywords
        || checkType == TextLookupCheckType::doesNotContainKeywords;
}

} // namespace

/** Prepared check data. Isn't changed after it's built. */
struct TextLookupField::Cache
{
    bool match(const QString& text, TextLookupCheckType checkType) const
    {
        if (!keywords)
            return false;

        switch (checkType)
        {
            case TextLookupCheckType::containsKeywords:
                return keywords->empty() || nx::vms::event::checkForKeywords(text, *keywords);
            case TextLookupCheckType::doesNotContainKeywords:
                return keywords->empty() || !nx::vms::event::checkForKeywords(text, *keywords);
            case TextLookupCheckType::inList:
                return !keywords->empty() && nx::vms::event::checkForKeywords(text, *keywords);
            case TextLookupCheckType::notInList:
                return keywords->empty() || !nx::vms::event::checkForKeywords(text, *keywords);
        }

        return false;
    }

    /** Lookup List Manager revision the list was taken at. */
    std::uint64_t revision = 0;

    /** The keywords, or the list entry values. Empty if there's no such generic list. */
    std::optional<QStringList> keywords;
};

TextLookupField::TextLookupField(
    common::SystemContext* context,
    const FieldDescriptor* descriptor)
    :
    EventFilterField{descriptor},
    common::SystemContextAware{context}
{
}

QString TextLookupField::value() const
{
    return m_value;
}

void TextLookupField::setValue(const QString& value)
{
    if (m_value == value)
        return;

    m_value = value;
    invalidateCache();
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

    m_checkType = type;
    invalidateCache();
    emit checkTypeChanged();
}

bool TextLookupField::match(const QVariant& eventValue) const
{
    return prepareCache()->match(eventValue.toString(), m_checkType);
}

std::shared_ptr<TextLookupField::Cache> TextLookupField::prepareCache() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    // The keywords don't depend on the Lookup Lists.
    if (m_cache && isKeywordCheck(m_checkType))
        return m_cache;

    // Read the revision before the list, so the cached list is at least as new as the revision.
    const auto revision = lookupListManager()->revision();
    if (m_cache && m_cache->revision == revision)
        return m_cache;

    auto cache = std::make_shared<Cache>();
    cache->revision = revision;

    if (isKeywordCheck(m_checkType))
    {
        cache->keywords = nx::vms::event::splitOnPureKeywords(m_value);
    }
    else if (NX_ASSERT(nx::Uuid::isUuidString(m_value), "Check type and value aren't compatible"))
    {
        const auto lookupList = lookupListManager()->lookupList(nx::Uuid{m_value});
        if (!lookupList.id.isNull()
            && NX_ASSERT(lookupList.objectTypeId.isEmpty(), "Supports only generic lists"))
        {
            QStringList values;
            for (const auto& entry: lookupList.entries)
                values.push_back(entry.begin()->second);

            cache->keywords = std::move(values);
        }
    }

    m_cache = cache;
    return cache;
}

void TextLookupField::invalidateCache()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_cache.reset();
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
