// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "generic_visitor.h"
#include "type_utils.h"

namespace nx::reflect {

struct ArrayOrderField
{
    std::string name;
    bool reverse = false;
    std::optional<std::size_t> variantIndex;
    std::vector<ArrayOrderField> fields;
};

namespace array_orderer_details {

template<typename T>
std::function<int(const T&, const T&)> makeComparator(const std::vector<ArrayOrderField>& fields);

template<typename T, typename F>
std::function<int(const T&, const T&)> makeComparator(F field, bool reverse);

template<typename T>
class Comparator: public nx::reflect::GenericVisitor<Comparator<T>>
{
public:
    Comparator(const std::vector<ArrayOrderField>& fields): m_fields{fields}
    {
        m_comparators.resize(m_fields.size());
    }

    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        for (size_t i = 0; i < m_fields.size(); ++i)
        {
            if (m_fields[i].name == field.name())
            {
                using Field = typename WrappedField::Type;
                if (m_fields[i].fields.empty())
                {
                    m_comparators[i] = makeComparator<T>(field, m_fields[i].reverse);
                }
                else
                {
                    auto comparator = makeComparator<Field>(m_fields[i].fields);
                    if (!comparator)
                        break;

                    m_comparators[i] =
                        [field, comparator = std::move(comparator)](const T& lhs, const T& rhs)
                        {
                            return comparator(field.get(lhs), field.get(rhs));
                        };
                }
                break;
            }
        }
    }

    std::function<int(const T&, const T&)> finish()
    {
        return
            [comparators = std::move(m_comparators)](const T& lhs, const T& rhs)
            {
                for (const auto& c: comparators)
                {
                    if (!c)
                        continue;

                    if (int r = c(lhs, rhs); r != 0)
                        return r;
                }
                return 0;
            };
    }

private:
    const std::vector<ArrayOrderField>& m_fields;
    std::vector<std::function<int(const T&, const T&)>> m_comparators;
};

template<typename T>
int compare(const T& lhs, const T& rhs, bool reverse)
{
    if constexpr (IsOptionalV<T>)
    {
        using Value = typename T::value_type;
        return compare(lhs.value_or(Value{}), rhs.value_or(Value{}), reverse);
    }
    else if constexpr (IsVariantV<T>)
    {
        if (lhs.index() != rhs.index() || lhs.valueless_by_exception())
            return 0;

        return std::visit(
            [reverse, &rhs](const auto& lhs)
            {
                return compare(lhs, std::get<std::decay_t<decltype(lhs)>>(rhs), reverse);
            },
            lhs);
    }
    else
    {
        if (reverse)
            return rhs < lhs ? -1 : lhs < rhs ? 1 : 0;
        return lhs < rhs ? -1 : rhs < lhs ? 1 : 0;
    }
}

template<typename T, typename F>
std::function<int(const T&, const T&)> makeComparator(F field, bool reverse)
{
    using Field = typename F::Type;
    if constexpr (!IsInstrumentedV<Field> && !IsContainerV<Field>)
    {
        return
            [field = std::move(field), reverse](const T& lhs, const T& rhs)
            {
                return compare(field.get(lhs), field.get(rhs), reverse);
            };
    }
    else
    {
        return {};
    }
}

template<typename Variant, std::size_t Index>
void makeVariantComparator(
    std::array<std::function<int(const Variant&, const Variant&)>, std::variant_size_v<Variant>>*
        comparators,
    std::array<std::vector<ArrayOrderField>, std::variant_size_v<Variant>>* fieldsPerIndex)
{
    if ((*fieldsPerIndex)[Index].empty())
        return;

    auto comparator = makeComparator<std::variant_alternative_t<Index, Variant>>(
        std::move((*fieldsPerIndex)[Index]));
    if (!comparator)
        return;

    (*comparators)[Index] =
        [comparator = std::move(comparator)](const Variant& lhs, const Variant& rhs)
        {
            return lhs.index() == Index && rhs.index() == Index
                ? comparator(std::get<Index>(lhs), std::get<Index>(rhs))
                : 0;
        };
}

template<typename Variant, std::size_t... Index>
void makeVariantComparators(
    std::array<std::function<int(const Variant&, const Variant&)>, std::variant_size_v<Variant>>*
        comparators,
    std::array<std::vector<ArrayOrderField>, std::variant_size_v<Variant>> fieldsPerIndex,
    std::index_sequence<Index...>)
{
    (makeVariantComparator<Variant, Index>(comparators, &fieldsPerIndex), ...);
}

template<typename T>
std::function<int(const T&, const T&)> makeComparator(const std::vector<ArrayOrderField>& fields)
{
    if (fields.empty())
        return {};

    if constexpr (IsOptionalV<T>)
    {
        using Value = typename T::value_type;
        auto comparator = makeComparator<Value>(fields);
        if (!comparator)
            return {};

        return
            [c = std::move(comparator)](const T& lhs, const T& rhs)
            {
                return c(lhs.value_or(Value{}), rhs.value_or(Value{}));
            };
    }
    else if constexpr (IsVariantV<T>)
    {
        std::array<std::vector<ArrayOrderField>, std::variant_size_v<T>> fieldsPerIndex;
        for (const auto& f: fields)
        {
            if (f.variantIndex < std::variant_size_v<T>)
                fieldsPerIndex[*f.variantIndex].emplace_back(f);
        }

        std::array<std::function<int(const T&, const T&)>, std::variant_size_v<T>> comparators;
        makeVariantComparators(&comparators, std::move(fieldsPerIndex),
            std::make_index_sequence<std::variant_size_v<T>>());
        return
            [comparators = std::move(comparators)](const T& lhs, const T& rhs)
            {
                for (const auto& c: comparators)
                {
                    if (!c)
                        continue;

                    if (int r = c(lhs, rhs); r != 0)
                        return r;
                }
                return 0;
            };
    }
    else if constexpr (IsInstrumentedV<T>)
    {
        return visitAllFields<T>(Comparator<T>{fields});
    }
    else
    {
        return {};
    }
}

template<typename T>
class Orderer: public nx::reflect::GenericVisitor<Orderer<T>>
{
public:
    Orderer(T* data, const std::vector<ArrayOrderField>& fields): m_data{data}, m_fields{fields} {}

    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        for (const auto& orderField: m_fields)
        {
            if (!orderField.fields.empty() && orderField.name == field.name())
            {
                if constexpr (HasRef<WrappedField>::value)
                {
                    order(&field.ref(m_data), orderField.fields);
                }
                else
                {
                    auto data = field.get(*m_data);
                    order(&data, orderField.fields);
                    field.set(m_data, std::move(data));
                }
            }
        }
    }

private:
    template<typename F, typename = void>
    struct HasRef { static constexpr bool value = false; };

    template<typename F>
    struct HasRef<F, detail::void_t<decltype(&F::ref)>> { static constexpr bool value = true; };

private:
    T* m_data;
    const std::vector<ArrayOrderField>& m_fields;
};

} // namespace array_orderer_details

template<typename T>
void order(T* data, const std::vector<ArrayOrderField>& fields)
{
    using namespace array_orderer_details;

    if (fields.empty())
        return;

    if constexpr (IsOptionalV<T>)
    {
        if (data->has_value())
            order(&data->value(), fields);
    }
    else if constexpr (IsVariantV<T>)
    {
        std::visit([&fields](auto& v) { order(&v, fields); }, *data);
    }
    else if constexpr (IsInstrumentedV<T>)
    {
        visitAllFields<T>(Orderer<T>{data, fields});
    }
    else if constexpr (IsContainerV<T>)
    {
        if constexpr (IsSequenceContainerV<T>)
        {
            if (auto c = makeComparator<typename T::value_type>(fields))
            {
                std::stable_sort(data->begin(), data->end(),
                    [&c](const auto& lhs, const auto& rhs) { return c(lhs, rhs) < 0; });
            }
            for (auto& value: *data)
                order(&value, fields);
        }
        else if constexpr (IsSetContainerV<T> || IsUnorderedSetContainerV<T>)
        {
            for (auto& value: *data)
                order(&value, fields);
        }
        else if constexpr (IsAssociativeContainerV<T> || IsUnorderedAssociativeContainerV<T>)
        {
            for (auto& [_, value]: *data)
                order(&value, fields);
        }
    }
}

} // namespace nx::reflect
