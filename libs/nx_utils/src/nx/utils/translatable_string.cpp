// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "translatable_string.h"

#include <nx/branding.h>
#include <nx/utils/i18n/translation_manager.h>
#include <nx/utils/serialization/qt_containers_reflect_json.h>

namespace nx {

const QString TranslatableString::kDefaultValueKey = "_";

TranslatableString::TranslatableString(const QString& value):
    m_value(value),
    m_defaultValue(value)
{
}

TranslatableString::TranslatableString(Provider provider):
    m_value(std::move(provider))
{
}

TranslatableString::TranslatableString(QMap<QString, QString> valuesByLocale):
    m_value(valuesByLocale)
{
    if (valuesByLocale.contains(kDefaultValueKey))
        m_defaultValue = valuesByLocale.value(kDefaultValueKey);
    else if (valuesByLocale.contains(nx::branding::defaultLocale()))
        m_defaultValue = valuesByLocale.value(nx::branding::defaultLocale());
    else if (!valuesByLocale.empty())
        m_defaultValue = valuesByLocale.first();
}

QString TranslatableString::value(QString locale) const
{
    if (const auto provider = std::get_if<Provider>(&m_value))
        return (*provider)();

    if (locale.isEmpty())
        locale = i18n::TranslationManager::getCurrentThreadLocale();

    if (locale.isEmpty())
        locale = nx::branding::defaultLocale();

    if (const auto valuesByLocale = std::get_if<ValueByLocale>(&m_value))
        return valuesByLocale->value(locale, m_defaultValue);

    return m_defaultValue;
}

nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& ctx,
    TranslatableString* data)
{
    // TranslatableString can be represented as a map from locale to translation or as a simple
    // string.
    if (ctx.value.IsString())
    {
        QString value;
        if (!nx::reflect::json::deserialize(ctx, &value))
        {
            return {
                false,
                "Can't parse a string for TranslatableString",
                nx::reflect::json_detail::getStringRepresentation(ctx.value)};
        }
        *data = TranslatableString(value);
    }
    else if (ctx.value.IsObject())
    {
        QMap<QString, QString> valuesByLocale;
        if (!nx::reflect::json::deserialize(ctx, &valuesByLocale))
        {
            return {
                false,
                "Can't parse a values map for TranslatableString",
                nx::reflect::json_detail::getStringRepresentation(ctx.value)};
        }
        *data = TranslatableString(valuesByLocale);
    }
    else if (!ctx.value.IsNull())
    {
        return {
            false,
            "Unsupported representation of the TranslatableString",
            nx::reflect::json_detail::getStringRepresentation(ctx.value)};
    }

    return nx::reflect::DeserializationResult(true);
}

} // namespace nx
