// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "json.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

#include <nx/utils/log/log_main.h>
#include <nx/utils/serialization/qjson.h>

#include "exception.h"

namespace nx::network::rest::json {

namespace details {

bool merge(
    QJsonValue* existingValue,
    const QJsonValue& incompleteValue,
    QString* outErrorMessage,
    const QString& fieldName);

bool merge(
    QJsonObject* object,
    const QJsonObject& incompleteObject,
    QString* outErrorMessage)
{
    auto field = object->begin();
    auto incompleteField = incompleteObject.constBegin();
    while (field != object->end() && incompleteField != incompleteObject.constEnd())
    {
        const int compare = field.key().compare(incompleteField.key());
        if (compare < 0)
        {
            NX_VERBOSE(NX_SCOPE_TAG, "    Untouched field \"%1\"", field.key());
            ++field;
        }
        else if (compare == 0)
        {
            QJsonValue value = field.value();
            if (!merge(&value, *incompleteField, outErrorMessage, field.key()))
                return false;
            NX_VERBOSE(NX_SCOPE_TAG, "    Replaced field \"%1\"", field.key());
            field.value() = value;
            ++field;
            ++incompleteField;
        }
        else
        {
            NX_VERBOSE(NX_SCOPE_TAG, "    Added field \"%1\"", incompleteField.key());
            object->insert(incompleteField.key(), *incompleteField);
            ++incompleteField;
        }
    }
    for (; incompleteField != incompleteObject.constEnd(); ++incompleteField)
    {
        NX_VERBOSE(NX_SCOPE_TAG, "    Added field \"%1\"", incompleteField.key());
        object->insert(incompleteField.key(), *incompleteField);
    }
    return true;
}

bool merge(
    QJsonValue* existingValue,
    const QJsonValue& incompleteValue,
    QString* outErrorMessage,
    const QString& fieldName)
{
    const QString fieldReference = fieldName.isEmpty() ? "" : NX_FMT(" field \"%1\"", fieldName);
    NX_VERBOSE(NX_SCOPE_TAG, "BEGIN merge%1", fieldReference);

    if (incompleteValue.type() == QJsonValue::Undefined) //< Missing field type, as per Qt doc.
    {
        NX_VERBOSE(NX_SCOPE_TAG, "    Incomplete value is missing - ignored");
        return true; //< leave recursion
    }

    switch (existingValue->type())
    {
        case QJsonValue::Null:
        case QJsonValue::Bool:
        case QJsonValue::Double:
        case QJsonValue::String:
        case QJsonValue::Array: //< Arrays treated as scalars - no items merging is performed.
            NX_VERBOSE(NX_SCOPE_TAG, "    Replace %1 with %2",
                QJson::toString(existingValue->type()), QJson::toString(incompleteValue.type()));
            *existingValue = incompleteValue;
            break;

        case QJsonValue::Object:
        {
            if (!incompleteValue.isObject())
            {
                NX_VERBOSE(NX_SCOPE_TAG, "    Replace object with %1",
                    QJson::toString(incompleteValue.type()));
                *existingValue = incompleteValue;
                break;
            }

            NX_VERBOSE(NX_SCOPE_TAG, "    Object - process recursively");
            QJsonObject object = existingValue->toObject();
            if (!merge(&object, incompleteValue.toObject(), outErrorMessage))
                return false;
            *existingValue = object; //< Write back possibly changed object.
            break;
        }

        default:
            NX_VERBOSE(NX_SCOPE_TAG, "    Unknown type - ignored");
    }

    NX_VERBOSE(NX_SCOPE_TAG, "END merge%1", fieldReference);
    return true;
}

namespace
{

struct FilterItems
{
    QMap<QString, FilterItems> items;
    QString value;
};

auto popFront(QString hierarchicalName)
{
    const int pos = hierarchicalName.indexOf('.');
    return std::make_pair(
        hierarchicalName.left(pos),
        (pos == -1) ? QString() : hierarchicalName.right(hierarchicalName.length() - pos - 1));
}

bool filterAny(const utils::DotNotationString& items, QJsonValue* result);

bool filterArray(const utils::DotNotationString& items, QJsonValue* result)
{
    bool filtered = false;
    QJsonArray values = result->toArray();
    for (int i = 0, n = values.size(); i < n; ++i)
    {
        QJsonValue value = values[i];
        if (filterAny(items, &value))
        {
            filtered = true;
            values.replace(i, value);
        }
    }
    if (filtered)
        *result = QJsonValue(values);
    return filtered;
}

bool filterObject(const utils::DotNotationString& items, QJsonValue* result)
{
    bool filtered = false;
    bool wildcard = items.contains("*");
    auto wildcardFilter = items.find("*");
    QJsonObject object = result->toObject();

    const auto filterNestedAndIncrement =
        [&object, &wildcardFilter](auto& filter, auto& field, bool addWildcard)
        {
            if (const auto& nested = filter.value(); !nested.empty())
            {
                utils::DotNotationString merged;
                if (addWildcard)
                {
                    merged = wildcardFilter.value();
                    merged.add(nested);
                }

                QJsonValue value = field.value();
                if (filterAny(addWildcard ? merged : nested, &value))
                {
                    if (value.isObject() && value.toObject().empty())
                    {
                        field = object.erase(field);
                    }
                    else
                    {
                        field.value() = value;
                        ++field;
                    }
                    return true;
                }
            }
            ++field;
            return false;
        };

    auto field = object.begin();
    auto filter = items.constBegin();
    while (field != object.end() && filter != items.constEnd())
    {
        if (filter.key() == "*")
        {
            ++filter;
            continue;
        }

        const int compare = field.key().compare(filter.key());
        if (compare < 0)
        {
            if (!wildcard)
            {
                field = object.erase(field);
                filtered = true;
            }
            else
            {
                if (filterNestedAndIncrement(wildcardFilter, field, false))
                    filtered = true;
            }
        }
        else if (compare == 0)
        {
            if (filterNestedAndIncrement(filter, field, wildcard))
                filtered = true;

            ++filter;
        }
        else
        {
            ++filter;
        }
    }
    while (field != object.end())
    {
        if (!wildcard)
        {
            field = object.erase(field);
            filtered = true;
        }
        else
        {
            if (filterNestedAndIncrement(wildcardFilter, field, false))
                filtered = true;
        }
    }
    if (filtered)
        *result = QJsonValue(object);
    return filtered;
}

bool filterAny(const utils::DotNotationString& items, QJsonValue* value)
{
    if (value->isArray())
        return filterArray(items, value);
    if (value->isObject())
        return filterObject(items, value);
    if (!items.empty())
        throw Exception::badRequest(NX_FMT("Unknown _with item(s): %1", items.keys().join(", ")));
    return false;
}

bool matchAny(const QMap<QString, FilterItems>& items, QJsonValue* result);

bool matchArray(const QMap<QString, FilterItems>& items, QJsonValue* result)
{
    QJsonArray values = result->toArray();
    QJsonArray newValues;
    for (int i = 0, n = values.size(); i < n; ++i)
    {
        QJsonValue value = values[i];
        if (matchAny(items, &value))
            newValues.push_back(value);
    }
    *result = QJsonValue(newValues);
    return !newValues.isEmpty();
}

bool matchObject(const QMap<QString, FilterItems>& items, QJsonValue* result)
{
    QJsonObject object = result->toObject();
    auto field = object.begin();
    auto filter = items.constBegin();
    while (field != object.end() && filter != items.constEnd())
    {
        const int compare = field.key().compare(filter.key());
        if (compare < 0)
        {
            ++field;
        }
        else if (compare == 0)
        {
            const auto& nested = filter.value().items;
            QJsonValue value = field.value();
            if (nested.empty()
                ? filter.value().value != Params::valueToString(std::move(value))
                : !matchAny(nested, &value))
            {
                *result = {};
                return false;
            }
            if (!value.isNull())
                field.value() = value;
            ++field;
            ++filter;
        }
        else
        {
            *result = {};
            return false;
        }
    }
    if (filter != items.constEnd())
    {
        *result = {};
        return false;
    }
    *result = object;
    return true;
}

bool matchAny(const QMap<QString, FilterItems>& items, QJsonValue* value)
{
    if (value->isArray())
        return matchArray(items, value);
    if (value->isObject())
        return matchObject(items, value);
    if (!items.empty())
        throw Exception::badRequest(NX_FMT("Unknown filter item(s): %1", items.keys().join(", ")));
    return false;
}

void filterToItems(QString key, QString value, QMap<QString, FilterItems>* items)
{
    auto [item, subitem] = popFront(std::move(key));
    FilterItems& nested = (*items)[item];
    if (!subitem.isEmpty())
        filterToItems(std::move(subitem), std::move(value), &nested.items);
    else
        nested.value = std::move(value);
}

void filterAny(const utils::DotNotationString& items, rapidjson::Value* result);

void filterArray(const utils::DotNotationString& items, rapidjson::Value* result)
{
    for (auto it = result->Begin(); it != result->End(); ++it)
        filterAny(items, &(*it));
}

void filterObject(const utils::DotNotationString& items, rapidjson::Value* result)
{
    auto wildcardFilter = items.find("*");
    const bool wildcard = wildcardFilter != items.end();

    auto field = result->MemberBegin();
    while (field != result->MemberEnd())
    {
        auto it = items.find(field->name.GetString());
        auto& filter = it != items.end() ? it : wildcardFilter;
        if (filter != items.end())
        {
            if (const auto& nested = filter.value(); !nested.empty())
            {
                if (wildcard)
                {
                    utils::DotNotationString merged;
                    merged = wildcardFilter.value();
                    merged.add(nested);
                    filterAny(merged, &field->value);
                }
                else
                {
                    filterAny(nested, &field->value);
                }
            }
            if (field->value.IsObject() && field->value.ObjectEmpty())
                field = result->EraseMember(field);
            else
                ++field;
        }
        else
        {
            field = result->EraseMember(field);
        }
    }
}

void filterAny(const utils::DotNotationString& items, rapidjson::Value* value)
{
    if (value->IsArray())
        return filterArray(items, value);
    if (value->IsObject())
        return filterObject(items, value);
    if (!items.empty())
        throw Exception::badRequest(NX_FMT("Unknown _with item(s): %1", items.keys().join(", ")));
}

bool processDefaultValue(
    const QJsonValue& defaultValue, QJsonValue* value, DefaultValueAction action);

bool processDefaultValue(
    const QJsonArray& defaultValue, QJsonValue* result, DefaultValueAction action)
{
    QJsonArray values = result->toArray();
    if (values.isEmpty())
        return true;

    bool isProcessed = false;
    QJsonArray output;
    for (int i = 0, n = values.size(); i < n; ++i)
    {
        QJsonValue value = values[i];
        if (value.isArray() || value.isObject())
        {
            for (int j = 0, k = defaultValue.size(); j < k; ++j)
            {
                if (processDefaultValue(defaultValue[j], &value, action))
                {
                    isProcessed = true;
                    break;
                }
            }
        }
        output.append(value);
    }
    if (isProcessed)
        *result = output;
    return isProcessed;
}

bool processDefaultValue(
    const QJsonObject& defaultValue, QJsonValue* result, DefaultValueAction action)
{
    QJsonObject object = result->toObject();
    bool isProcessed = object.isEmpty();
    auto field = object.begin();
    auto filter = defaultValue.constBegin();
    const bool matchAny = filter != defaultValue.constEnd() && filter.key().isEmpty();
    while (field != object.end() && filter != defaultValue.constEnd())
    {
        const int compare = matchAny ? 0 : field.key().compare(filter.key());
        if (compare == 0)
        {
            QJsonValue value = field.value();
            if (processDefaultValue(filter.value(), &value, action))
            {
                isProcessed = true;
                switch (action)
                {
                    case DefaultValueAction::removeEqual:
                        if (value.isNull()
                            || (value.isArray() && value.toArray().isEmpty())
                            || (value.isObject() && value.toObject().isEmpty()))
                        {
                            field = object.erase(field);
                            break;
                        }
                        [[fallthrough]];
                    case DefaultValueAction::appendMissing:
                        field.value() = value;
                        ++field;
                        break;
                }
            }
            else
            {
                ++field;
            }
            if (!matchAny)
                ++filter;
            continue;
        }
        if (compare < 0)
        {
            switch (action)
            {
                case DefaultValueAction::removeEqual:
                    if ((field->isArray() && field->toArray().isEmpty())
                        || (field->isObject() && field->toObject().isEmpty()))
                    {
                        field = object.erase(field);
                        isProcessed = true;
                        continue;
                    }
                    break;
                case DefaultValueAction::appendMissing:
                    break;
            }
            ++field;
            continue;
        }
        switch (action)
        {
            case DefaultValueAction::removeEqual:
                break;
            case DefaultValueAction::appendMissing:
                isProcessed = true;
                field = ++object.insert(filter.key(), filter.value());
                break;
        }
        ++filter;
    }
    switch (action)
    {
        case DefaultValueAction::removeEqual:
            while (field != object.end())
            {
                if ((field->isArray() && field->toArray().isEmpty())
                    || (field->isObject() && field->toObject().isEmpty()))
                {
                    field = object.erase(field);
                    isProcessed = true;
                }
                else
                {
                    ++field;
                }
            }
            if (isProcessed)
                *result = object;
            break;
        case DefaultValueAction::appendMissing:
            if (!matchAny && filter != defaultValue.constEnd())
            {
                isProcessed = true;
                object.insert(filter.key(), filter.value());
                for (++filter; filter != defaultValue.constEnd(); ++filter)
                    object.insert(filter.key(), filter.value());
            }
            if (isProcessed)
                *result = QJsonValue(object);
            break;
    }
    return isProcessed;
}

bool processDefaultValue(
    const QJsonValue& defaultValue, QJsonValue* value, DefaultValueAction action)
{
    static const QJsonArray kDefaultArray = QJsonArray{QJsonValue()};
    static const QJsonObject kDefaultObject = QJsonObject{{QString(), QJsonValue()}};
    if (defaultValue.isNull())
    {
        switch (action)
        {
            case DefaultValueAction::removeEqual:
                break;
            case DefaultValueAction::appendMissing:
                return false;
        }
        switch (value->type())
        {
            case QJsonValue::Null:
                return false;
            case QJsonValue::Bool:
                if (value->toBool())
                    return false;
                break;
            case QJsonValue::Double:
                if (value->toDouble() != 0)
                    return false;
                break;
            case QJsonValue::String:
                if (!value->toString().isEmpty())
                    return false;
                break;
            case QJsonValue::Array:
                return processDefaultValue(kDefaultArray, value, action);
            case QJsonValue::Object:
                return processDefaultValue(kDefaultObject, value, action);
            case QJsonValue::Undefined:
                break;
        }
    }
    else
    {
        if (value->isArray())
        {
            return NX_ASSERT(defaultValue.isArray())
                && processDefaultValue(defaultValue.toArray(), value, action);
        }
        if (value->isObject())
        {
            return NX_ASSERT(defaultValue.isObject())
                && processDefaultValue(defaultValue.toObject(), value, action);
        }
        switch (action)
        {
            case DefaultValueAction::removeEqual:
                if (defaultValue != *value)
                    return false;
                break;
            case DefaultValueAction::appendMissing:
                if (value->isNull())
                {
                    *value = defaultValue;
                    return true;
                }
                return false;
        }
    }
    *value = QJsonValue();
    return true;
}

static const std::array<QJsonValue, QJsonValue::Object + 1> kDefaultJsonValues = {
    QJsonValue(), QJsonValue(false), QJsonValue(0), QString(), QJsonArray(), QJsonObject()};

} // anonymous namespace

utils::DotNotationString extractWithParam(Params* params)
{
    utils::DotNotationString result;
    const QString withParam = params->take("_with");
    if (!withParam.isEmpty())
    {
        auto withs = withParam.split(',');
        for (auto& with: withs)
        {
            if (with.isEmpty())
                throw Exception::badRequest("Empty item in _with parameter");
            result.add(std::move(with));
        }
    }
    return result;
}

void filter(
    QJsonValue* value, const QJsonValue* defaultValue, DefaultValueAction action, Params params, utils::DotNotationString withParam)
{
    if (!params.empty())
    {
        QMap<QString, FilterItems> items;
        for (auto [k, v]: params.keyValueRange())
        {
            if (k.isEmpty())
                throw Exception::badRequest("Empty name of parameter");
            if (!k.startsWith(QChar('_')))
                filterToItems(k, v, &items);
        }
        if (!items.empty())
            matchAny(items, value);
    }
    if (!withParam.isEmpty())
        filterAny(withParam, value);
    if (defaultValue)
    {
        const QJsonValue::Type type = value->type();
        if (type != QJsonValue::Undefined)
        {
            processDefaultValue(*defaultValue, value, action);
            switch (action)
            {
                case DefaultValueAction::removeEqual:
                    break;
                case DefaultValueAction::appendMissing:
                    if (!withParam.isEmpty())
                        filterAny(withParam, value);
                    break;
            }
            if (value->isNull())
                *value = kDefaultJsonValues[type];
        }
    }
}

void filter(rapidjson::Document* value, nx::utils::DotNotationString with)
{
    if (!with.isEmpty())
        filterAny(with, value);
}

} // namespace details

QJsonValue serialized(rapidjson::Value& data)
{
    QJsonValue value;
    const auto r{
        nx::reflect::json::deserialize(nx::reflect::json::DeserializationContext{data}, &value)};
    NX_ASSERT(r, "%1", r.toString());
    return value;
}

QByteArray serialized(const QJsonValue& value, Qn::SerializationFormat format)
{
    if (format == Qn::SerializationFormat::urlEncoded
        || format == Qn::SerializationFormat::urlQuery)
    {
        if (!value.isObject())
        {
            throw Exception::unsupportedMediaType(
                NX_FMT("Unsupported format for non-object data: %1", format));
        }

        const auto encode = format == Qn::SerializationFormat::urlEncoded
            ? QUrl::FullyEncoded
            : QUrl::PrettyDecoded;
        return Params::fromJson(value.toObject()).toUrlQuery().query(encode).toUtf8();
    }
    auto serializedData = trySerialize(value, format);
    if (!serializedData.has_value())
    {
        throw nx::network::rest::Exception::unsupportedMediaType(
            NX_FMT("Unsupported format: %1", format));
    }
    return *serializedData;
}

QByteArray serialized(const rapidjson::Value& value, Qn::SerializationFormat format)
{
    switch (format)
    {
        case Qn::SerializationFormat::urlEncoded:
        case Qn::SerializationFormat::urlQuery:
            if (!value.IsObject())
            {
                throw Exception::unsupportedMediaType(
                    NX_FMT("Unsupported format for non-object data: %1", format));
            }
            return Params::fromJson(value).toUrlQuery().query(
                format == Qn::SerializationFormat::urlEncoded
                    ? QUrl::FullyEncoded
                    : QUrl::PrettyDecoded).toUtf8();

        case Qn::SerializationFormat::json:
        case Qn::SerializationFormat::unsupported:
            return QByteArray::fromStdString(
                nx::reflect::json_detail::getStringRepresentation(value));

        case Qn::SerializationFormat::csv:
            return QnCsv::serialized(value);

        case Qn::SerializationFormat::xml:
            return QnXml::serialized(value, "reply");

        default:
            throw Exception::unsupportedMediaType();
    }
}

} // namespace nx::network::rest::json
