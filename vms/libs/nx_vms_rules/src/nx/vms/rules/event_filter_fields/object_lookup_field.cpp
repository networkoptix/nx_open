// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_lookup_field.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>

#include <analytics/db/text_search_utils.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/rules/utils/openapi_doc.h>

namespace nx::vms::rules {

namespace {

using Attributes = nx::common::metadata::Attributes;

bool entryMatches(const Attributes& attributes, const nx::vms::api::LookupListEntry& entry)
{
    return std::ranges::all_of(entry,
        [&attributes](const auto& pair)
        {
            return pair.second.isEmpty() || std::ranges::any_of(attributes,
                [&pair](const auto& attribute)
                {
                    return attribute.name == pair.first && attribute.value == pair.second;
                });
        });
}

/**
 * Returns whether the list contains an entry that matches the given attributes.
 * For a generic list, it checks that at least one value matches the attribute.
 */
bool checkForListEntries(
    const api::LookupListData& lookupList, const nx::common::metadata::Attributes& attributes)
{
    if (attributes.empty())
        return false;

    // Lookup list is Generic one.
    if (lookupList.objectTypeId.isEmpty())
    {
        for (const auto& attribute: attributes)
        {
            bool hasAtLeastOneMatch = std::ranges::any_of(lookupList.entries,
                [&](const std::map<QString, QString>& entry)
                {
                    return entry.begin()->second == attribute.value;
                });

            if (hasAtLeastOneMatch)
                return true;
        }

        return false;
    }

    for (const auto& entry: lookupList.entries)
    {
        if (entryMatches(attributes, entry))
            return true;
    }

    return false;
}

} // namespace

/** Prepared check data. Isn't changed after it's built. */
struct ObjectLookupField::Cache
{
    bool match(const Attributes& attributes, ObjectLookupCheckType checkType) const
    {
        if (checkType == ObjectLookupCheckType::hasAttributes)
            return matcher && matcher->matchAttributes(attributes);

        if (!list)
            return false;

        const auto hasEntry = checkForListEntries(*list, attributes);
        return checkType == ObjectLookupCheckType::inList ? hasEntry : !hasEntry;
    }

    /** Lookup List Manager revision the list was taken at. */
    std::uint64_t revision = 0;

    /** For the hasAttributes check type. Empty if the value isn't valid. */
    std::optional<analytics::db::TextMatcher> matcher;

    /** For the list check types. Empty if there's no such list. */
    std::optional<api::LookupListData> list;
};

ObjectLookupField::ObjectLookupField(
    common::SystemContext* context,
    const FieldDescriptor* descriptor)
    :
    EventFilterField{descriptor},
    common::SystemContextAware{context}
{
}

QString ObjectLookupField::value() const
{
    return m_value;
}

void ObjectLookupField::setValue(const QString& value)
{
    if (m_value == value)
        return;

    m_value = value;
    invalidateCache();
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

    m_checkType = type;
    invalidateCache();
    emit checkTypeChanged();
}

bool ObjectLookupField::match(const QVariant& eventValue) const
{
    return prepareCache()->match(eventValue.value<Attributes>(), m_checkType);
}

std::shared_ptr<ObjectLookupField::Cache> ObjectLookupField::prepareCache() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    // The attribute filter doesn't depend on the Lookup Lists.
    if (m_cache && m_checkType == ObjectLookupCheckType::hasAttributes)
        return m_cache;

    // Read the revision before the list, so the cached list is at least as new as the revision.
    const auto revision = lookupListManager()->revision();
    if (m_cache && m_cache->revision == revision)
        return m_cache;

    auto cache = std::make_shared<Cache>();
    cache->revision = revision;

    if (m_checkType == ObjectLookupCheckType::hasAttributes)
    {
        if (NX_ASSERT(m_value.isEmpty() || !nx::Uuid::isUuidString(m_value),
                "Check type and value aren't compatible"))
        {
            cache->matcher.emplace(m_value);
        }
    }
    else if (NX_ASSERT(nx::Uuid::isUuidString(m_value), "Check type and value aren't compatible"))
    {
        auto lookupList = lookupListManager()->lookupList(nx::Uuid{m_value});
        if (!lookupList.id.isNull())
            cache->list = std::move(lookupList);
    }

    m_cache = cache;
    return cache;
}

void ObjectLookupField::invalidateCache()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_cache.reset();
}

QJsonObject ObjectLookupField::openApiDescriptor(const QVariantMap&)
{
    auto result = utils::constructOpenApiDescriptor<ObjectLookupField>();
    result[utils::kDescriptionProperty] =
        "This field filters objects by matching <b>Object Attribute Filter Expression</b> "
        "(<code>checkType</code>: <b>hasAttributes</b>)"
        "or by referencing a <b>lookup list</b> identified by its ID"
        "(<code>checkType</code>: <b>inList</b> or <b>notInList</b>).<br/>"

        "<p>To prepare <b>Object Attribute Filter Expression</b> <code>value</code> "
        "for <code>checkType</code> <b>hasAttributes</b>:</p>"
        "<ul>"
        "<li>List object attributes and their corresponding conditions "
        "in the format <code>Attribute=Value</code> or use comparison operators "
        "like <code>&lt;</code>, <code>&gt;</code>, <code>!=</code>, <code>&lt;=</code>, "
        "<code>&gt;=</code> or <code>=</code>.</li>"
        "<li>Each condition describes an attribute of the object "
        "(e.g., <code>Gender=Woman</code>, <code>Age=Adult</code>, <code>Height&lt;170</code>).</li>"
        "<li>Separate conditions using either spaces or commas.</li>"
        "<li>Note that the maximum number of conditions is 64.</li>"
        "</ul>"
        "<p>Example expressions:</p>"
        "<ul>"
        "<li><code>Gender=Woman Age=Adult Height&lt;170</code></li>"
        "<li><code>Gender!=Man,Age!=Young,Height==170,Glasses=Present</code></li>"
        "</ul><br/>"

        "Set the <b>UUID</b> of the Lookup List as the <code>value</code> "
        "for the <code>checkType</code> <b>inList</b> "
        "or <b>notInList</b>.<br/>"
        "To find the required Lookup List, "
        "refer to the response of the request to <code>/rest/v4/lookuplists.</code>";
    return result;
}

} // namespace nx::vms::rules
