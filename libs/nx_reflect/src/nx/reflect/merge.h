// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "from_string.h"
#include "generic_visitor.h"
#include "serialization_utils.h"
#include "type_utils.h"

namespace nx::reflect {

template<typename T>
void merge(
    T* origin,
    T* updated,
    DeserializationResult::Fields fields,
    DeserializationResult::Fields* originFields = nullptr,
    bool replaceAssociativeContainer = false);

namespace merge_details {

template<typename T>
T keyFromName(std::string&& name)
{
    if constexpr (std::is_same_v<std::string, T>)
        return name;
    else
        return fromString<T>(name);
}

template<typename T>
T keyFromName(const std::string& name)
{
    return fromString<T>(name);
}

inline DeserializationResult::Fields* nextOriginFields(
    std::string_view name, DeserializationResult::Fields* originFields)
{
    if (!originFields)
        return nullptr;

    auto next = std::find_if(originFields->begin(), originFields->end(),
        [&name](const auto& f) { return f.name == name; });
    return next == originFields->end() ? nullptr : &next->fields;
}

template<typename T>
class MergeVisitor: public GenericVisitor<MergeVisitor<T>>
{
public:
    MergeVisitor(
        T* origin,
        T* updated,
        DeserializationResult::Fields fields,
        DeserializationResult::Fields* originFields,
        bool replaceAssociativeContainer)
        :
        m_origin(origin),
        m_updated(updated),
        m_fields(std::move(fields)),
        m_originFields(originFields),
        m_replaceAssociativeContainer(replaceAssociativeContainer)
    {
    }

    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        auto name = field.name();
        auto next = std::find_if(
            m_fields.begin(), m_fields.end(), [name](auto f) { return f.name == name; });
        if (next == m_fields.end())
            return;

        constexpr bool useOriginFields =
            IsOptionalV<typename WrappedField::Type> || IsVariantV<typename WrappedField::Type>;

        if constexpr (HasRef<WrappedField>::value)
        {
            merge(
                &field.ref(m_origin),
                &field.ref(m_updated),
                std::move(next->fields),
                useOriginFields ? m_originFields : nextOriginFields(name, m_originFields),
                m_replaceAssociativeContainer);
        }
        else
        {
            auto origin = field.get(*m_origin);
            auto updated = field.get(*m_updated);
            merge(
                &origin,
                &updated,
                std::move(next->fields),
                useOriginFields ? m_originFields : nextOriginFields(name, m_originFields),
                m_replaceAssociativeContainer);
            field.set(m_origin, std::move(origin));
        }
    }

private:
    template<typename F, typename = void>
    struct HasRef { static constexpr bool value = false; };

    template<typename F>
    struct HasRef<F, detail::void_t<decltype(&F::ref)>> { static constexpr bool value = true; };

private:
    T* m_origin;
    T* m_updated;
    DeserializationResult::Fields m_fields;
    DeserializationResult::Fields* m_originFields;
    bool m_replaceAssociativeContainer;
};

template<typename T>
void replace(
    T* origin,
    T* updated,
    DeserializationResult::Fields fields,
    DeserializationResult::Fields* originFields)
{
    if (originFields)
        *originFields = std::move(fields);
    *origin = std::move(*updated);
}

} // namespace merge_details

template<typename T>
void mergeAssociativeContainer(
    T* origin,
    T* updated,
    DeserializationResult::Fields fields,
    DeserializationResult::Fields* originFields,
    bool replaceAssociativeContainer)
{
    for (auto& field: fields)
    {
        auto key = originFields
            ? merge_details::keyFromName<typename T::key_type>(field.name)
            : merge_details::keyFromName<typename T::key_type>(std::move(field.name));
        auto updatedField = updated->find(key);
        if (updatedField == updated->end())
            continue;

        auto originField = origin->find(key);
        if (originField == origin->end())
        {
            origin->emplace(std ::move(key), std::move(updatedField->second));
            if (originFields)
                originFields->push_back(std::move(field));
            continue;
        }

        merge(&originField->second, &updatedField->second, std::move(field.fields),
            merge_details::nextOriginFields(field.name, originFields), replaceAssociativeContainer);
    }
}

template<typename T>
void merge(
    T* origin,
    T* updated,
    DeserializationResult::Fields fields,
    DeserializationResult::Fields* originFields,
    bool replaceAssociativeContainer)
{
    if constexpr (IsOptionalV<T>)
    {
        if (!origin->has_value() || !updated->has_value())
            return merge_details::replace(origin, updated, std::move(fields), originFields);

        merge(&origin->value(), &updated->value(), std::move(fields),
            originFields, replaceAssociativeContainer);
    }
    else if constexpr (IsVariantV<T>)
    {
        if (origin->index() != updated->index())
            return merge_details::replace(origin, updated, std::move(fields), originFields);

        std::visit(
            [updated, &fields, originFields, replaceAssociativeContainer](auto& originValue) mutable
            {
                auto& updatedValue = std::get<std::decay_t<decltype(originValue)>>(*updated);
                merge(&originValue, &updatedValue, std::move(fields),
                    originFields, replaceAssociativeContainer);
            },
            *origin);
    }
    else if constexpr (IsInstrumentedV<T>)
    {
        if (!fields.empty())
        {
            nxReflectVisitAllFields(origin, merge_details::MergeVisitor{
                origin, updated, std::move(fields), originFields, replaceAssociativeContainer});
        }
    }
    else if constexpr ((IsAssociativeContainerV<T> && !IsSetContainerV<T>)
        || (IsUnorderedAssociativeContainerV<T> && !IsUnorderedSetContainerV<T>))
    {
        if (replaceAssociativeContainer)
            return merge_details::replace(origin, updated, std::move(fields), originFields);

        if (!fields.empty())
        {
            mergeAssociativeContainer(
                origin, updated, std::move(fields), originFields, /*replaceAssociativeContainer*/ false);
        }
    }
    else
    {
        merge_details::replace(origin, updated, std::move(fields), originFields);
    }
}

} // namespace nx::reflect
