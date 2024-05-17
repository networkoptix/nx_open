// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <variant>

#include <QtCore/QMap>
#include <QtCore/QString>

#include <nx/reflect/json.h>

namespace nx {

/**
 * String representation, capable of loading translations from json or storing translatable string
 * from code.
 */
class NX_UTILS_API TranslatableString
{
public:
    /**
     * When deserialized from the json format, default key means the value which should be used
     * if the requested locale was not found.
     */
    static const QString kDefaultValueKey; // Value is "_"

    TranslatableString() = default;

    /** Initialize from a single value, which will always be returned. */
    explicit TranslatableString(const QString& value);

    using Provider = std::function<QString()>;
    TranslatableString(Provider provider);

    using ValueByLocale = QMap<QString, QString>;
    TranslatableString(ValueByLocale valuesByLocale);

    /**
     * Return the string, translated to the given locale. Empty locale means the current one.
     */
    QString value(QString locale = {}) const;

    QString operator()() const { return value(); };

    operator QString() const { return value(); }

    inline bool operator==(const TranslatableString& other) const
    {
        return value() == other.value();
    }

    inline bool operator==(const QString& other) const
    {
        return value() == other;
    }

private:
    std::variant<QString, ValueByLocale, Provider> m_value;
    QString m_defaultValue;
};

#define NX_DYNAMIC_TRANSLATABLE(func) nx::TranslatableString([] { return func;})

NX_UTILS_API nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& ctx,
    TranslatableString* data);

NX_UTILS_API void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const TranslatableString& value);

} // namespace nx
