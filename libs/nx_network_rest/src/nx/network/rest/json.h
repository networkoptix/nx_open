// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QJsonValue>

#include <nx/fusion/serialization/json.h>
#include <nx/reflect/generic_visitor.h>
#include <nx/reflect/serialization_utils.h>
#include <nx/utils/dot_notation_string.h>
#include <nx/utils/std/algorithm.h>

#include "params.h"

namespace nx::network::rest::json
{

enum class DefaultValueAction: bool
{
    removeEqual,
    appendMissing,
};

template<typename T>
void merge(T* origin, T* updated, const nx::reflect::DeserializationResult::Fields& fields);

namespace details {

NX_NETWORK_REST_API bool merge(
    QJsonValue* existingValue,
    const QJsonValue& incompleteValue,
    QString* outErrorMessage,
    const QString& fieldName);

NX_NETWORK_REST_API void filter(
    QJsonValue* value,
    const QJsonValue* defaultValue,
    DefaultValueAction action,
    Params filters = {},
    nx::utils::DotNotationString with = {});

NX_NETWORK_REST_API nx::utils::DotNotationString extractWithParam(Params* filters);

template<typename T>
void mergeAssociativeContainer(
    T* origin, T* updated, const nx::reflect::DeserializationResult::Fields& fields)
{
    for (const auto& field: fields)
    {
        auto updatedField = updated->find(field.name);
        if (!NX_ASSERT(updatedField != updated->end()))
            continue;

        auto originField = origin->find(field.name);
        if (originField == origin->end())
        {
            (*origin)[field.name] = updatedField->second;
            continue;
        }

        nx::network::rest::json::merge(&originField->second, &updatedField->second, field.fields);
    }
}

template<typename T>
class MergeVisitor: public nx::reflect::GenericVisitor<MergeVisitor<T>>
{
public:
    MergeVisitor(T* origin, T* updated, const nx::reflect::DeserializationResult::Fields& fields):
        m_origin(origin), m_updated(updated), m_fields(fields)
    {
    }

    template<typename Field>
    void visitField(Field& field)
    {
        using namespace nx::reflect;

        auto next =
            nx::utils::find_if(m_fields, [name = field.name()](auto f) { return f.name == name; });
        if (!next)
            return;

        using Type = typename Field::Type;
        if constexpr (IsOptionalV<Type>)
        {
            auto origin = field.get(*m_origin);
            auto updated = field.get(*m_updated);
            if (!origin || !updated)
                return field.set(m_origin, std::move(updated));

            auto& originValue = origin.value();
            auto& updatedValue = updated.value();
            nx::network::rest::json::merge(&originValue, &updatedValue, next->fields);
            field.set(m_origin, std::move(origin));
        }
        else if constexpr (IsVariantV<Type>)
        {
            auto origin = field.get(*m_origin);
            auto updated = field.get(*m_updated);
            if (origin.index() != updated.index())
                return field.set(m_origin, std::move(updated));

            std::visit(
                [&field, &updated, next](auto& originValue)
                {
                    auto& updatedValue = std::get<std::decay_t<decltype(originValue)>>(updated);
                    nx::network::rest::json::merge(&originValue, &updatedValue, next->fields);
                },
                origin);
            field.set(m_origin, std::move(origin));
        }
        else if constexpr (IsInstrumentedV<Type>)
        {
            if (next->fields.empty())
                return;

            auto origin = field.get(*m_origin);
            auto updated = field.get(*m_updated);
            nx::network::rest::json::merge(&origin, &updated, next->fields);
            field.set(m_origin, std::move(origin));
        }
        else if constexpr ((IsAssociativeContainerV<Type> && !IsSetContainerV<Type>)
            || (IsUnorderedAssociativeContainerV<Type> && !IsUnorderedSetContainerV<Type>) )
        {
            if (next->fields.empty())
                return;

            Type origin = field.get(*m_origin);
            Type updated = field.get(*m_updated);
            mergeAssociativeContainer(&origin, &updated, next->fields);
            field.set(m_origin, std::move(origin));
        }
        else
        {
            field.set(m_origin, field.get(*m_updated));
        }
    }

private:
    T* m_origin;
    T* m_updated;
    const nx::reflect::DeserializationResult::Fields& m_fields;
};

} // namespace details

template<typename T>
void merge(T* origin, T* updated, const nx::reflect::DeserializationResult::Fields& fields)
{
    using namespace nx::reflect;

    if constexpr (IsOptionalV<T>)
    {
        if (!origin->has_value() || !updated->has_value())
        {
            *origin = *updated;
            return;
        }

        auto& originValue = origin->value();
        auto& updatedValue = updated->value();
        merge(&originValue, &updatedValue, fields);
    }
    else if constexpr (IsVariantV<T>)
    {
        if (origin->index() != updated->index())
        {
            *origin = *updated;
            return;
        }

        std::visit(
            [&updated, &fields](auto& originValue)
            {
                auto& updatedValue = std::get<std::decay_t<decltype(originValue)>>(*updated);
                nx::network::rest::json::merge(&originValue, &updatedValue, fields);
            },
            *origin);
    }
    else if constexpr (IsInstrumentedV<T>)
    {
        if (!fields.empty())
            nxReflectVisitAllFields(origin, details::MergeVisitor{origin, updated, fields});
    }
    else if constexpr ((IsAssociativeContainerV<T> && !IsSetContainerV<T>)
        || (IsUnorderedAssociativeContainerV<T> && !IsUnorderedSetContainerV<T>) )
    {
        if (!fields.empty())
            details::mergeAssociativeContainer(origin, updated, fields);
    }
    else
    {
        *origin = *updated;
    }
}

template<typename Output, typename Input>
bool merge(
    Output* requestedValue,
    const Input& existingValue,
    const QJsonValue& incompleteValue,
    QString* outErrorMessage,
    bool chronoSerializedAsDouble)
{
    QnJsonContext jsonContext;
    jsonContext.setChronoSerializedAsDouble(chronoSerializedAsDouble);

    QJsonValue jsonValue;
    QJson::serialize(&jsonContext, existingValue, &jsonValue);
    if (!details::merge(&jsonValue, incompleteValue, outErrorMessage, QString()))
        return false;

    jsonContext.setStrictMode(true);
    jsonContext.deserializeReplacesExistingOptional(false);
    if (QJson::deserialize(&jsonContext, jsonValue, requestedValue))
        return true;

    *outErrorMessage = "Unable to deserialize merged Json data to destination object.";
    return false;
}

template<typename T>
inline QJsonValue defaultValue()
{
    QJsonValue value;
    QnJsonContext jsonContext;
    jsonContext.setSerializeMapToObject(true);
    jsonContext.setOptionalDefaultSerialization(true);
    jsonContext.setChronoSerializedAsDouble(true);
    QJson::serialize(&jsonContext, T(), &value);
    return value;
}

template<typename T>
inline QJsonValue serialized(const T& data)
{
    QnJsonContext jsonContext;
    jsonContext.setSerializeMapToObject(true);
    jsonContext.setChronoSerializedAsDouble(true);
    QJsonValue value;
    QJson::serialize(&jsonContext, data, &value);
    return value;
}

// throws exception if can't process all withs
template<typename T>
QJsonValue filter(
    const T& data, Params filters = {},
    DefaultValueAction defaultValueAction = DefaultValueAction::appendMissing)
{
    auto value = serialized(data);
    auto with = details::extractWithParam(&filters);
    static const auto defaultJson = defaultValue<T>();
    details::filter(&value, &defaultJson, defaultValueAction, std::move(filters), std::move(with));
    return value;
}

} // namespace nx::network::rest::json
