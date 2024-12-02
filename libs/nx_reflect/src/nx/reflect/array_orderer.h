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

struct ArrayOrder
{
    std::string name;
    bool reverse = false;
    std::optional<std::size_t> variantIndex;
    std::vector<ArrayOrder> fields;
};

namespace array_orderer_details {

template<typename T>
std::function<int(const T&, const T&)> makeComparator(const ArrayOrder& order);

template<typename T>
class Comparator: public nx::reflect::GenericVisitor<Comparator<T>>
{
public:
    Comparator(const ArrayOrder& order): m_order{order}
    {
        m_comparators.resize(m_order.fields.size());
    }

    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        for (size_t i = 0; i < m_order.fields.size(); ++i)
        {
            if (m_order.fields[i].name == field.name())
            {
                if (auto c = makeComparator<typename WrappedField::Type>(m_order.fields[i]))
                {
                    m_comparators[i] =
                        [field, c = std::move(c)](const T& lhs, const T& rhs)
                        {
                            return c(field.get(lhs), field.get(rhs));
                        };
                }
                break;
            }
        }
    }

    std::function<int(const T&, const T&)> finish()
    {
        std::erase_if(m_comparators, [](const auto& c) { return !c; });
        if (m_comparators.empty())
            return {};

        return
            [comparators = std::move(m_comparators)](const T& lhs, const T& rhs)
            {
                for (const auto& c: comparators)
                {
                    if (int r = c(lhs, rhs); r != 0)
                        return r;
                }
                return 0;
            };
    }

private:
    const ArrayOrder& m_order;
    std::vector<std::function<int(const T&, const T&)>> m_comparators;
};

template<typename Variant, std::size_t Index>
void makeVariantComparator(
    std::vector<std::function<int(const Variant&, const Variant&)>>* comparators,
    const std::array<ArrayOrder, std::variant_size_v<Variant>>& orderPerIndex)
{
    if (auto c = makeComparator<std::variant_alternative_t<Index, Variant>>(orderPerIndex[Index]))
    {
        comparators->emplace_back(
            [c = std::move(c)](const Variant& lhs, const Variant& rhs)
            {
                return lhs.index() == Index && rhs.index() == Index
                    ? c(std::get<Index>(lhs), std::get<Index>(rhs))
                    : 0;
            });
    }
}

template<typename Variant, std::size_t... Index>
void makeVariantComparators(
    std::vector<std::function<int(const Variant&, const Variant&)>>* comparators,
    const std::array<ArrayOrder, std::variant_size_v<Variant>>& orderPerIndex,
    std::index_sequence<Index...>)
{
    (makeVariantComparator<Variant, Index>(comparators, orderPerIndex), ...);
}

template<typename T>
std::function<int(const T&, const T&)> makeComparator(const ArrayOrder& order)
{
    if constexpr (std::is_same_v<std::nullptr_t, T>)
    {
        return {};
    }
    else if constexpr (IsOptionalV<T>)
    {
        using Value = typename T::value_type;
        auto comparator = makeComparator<Value>(order);
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
        std::array<ArrayOrder, std::variant_size_v<T>> orderPerIndex;
        for (const auto& f: order.fields)
        {
            if (f.variantIndex < std::variant_size_v<T>)
                orderPerIndex[*f.variantIndex] = f;
        }
        std::vector<std::function<int(const T&, const T&)>> comparators;
        comparators.reserve(std::variant_size_v<T>);
        makeVariantComparators(&comparators, orderPerIndex,
            std::make_index_sequence<std::variant_size_v<T>>());
        if (comparators.empty())
            return {};

        return
            [comparators = std::move(comparators)](const T& lhs, const T& rhs)
            {
                for (const auto& c: comparators)
                {
                    if (int r = c(lhs, rhs); r != 0)
                        return r;
                }
                return 0;
            };
    }
    else if constexpr (IsInstrumentedV<T>)
    {
        return visitAllFields<T>(Comparator<T>{order});
    }
    else if constexpr (IsContainerV<T>)
    {
        return {};
    }
    else
    {
        return
            [reverse = order.reverse](const T& lhs, const T& rhs)
            {
                if (reverse)
                    return std::less<T>{}(rhs, lhs) ? -1 : std::less<T>{}(lhs, rhs) ? 1 : 0;
                return std::less<T>{}(lhs, rhs) ? -1 : std::less<T>{}(rhs, lhs) ? 1 : 0;
            };
    }
}

template<typename T>
class Orderer: public nx::reflect::GenericVisitor<Orderer<T>>
{
public:
    Orderer(T* data, const ArrayOrder& order): m_data{data}, m_order{order} {}

    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        for (const auto& f: m_order.fields)
        {
            if (f.name == field.name())
            {
                if constexpr (HasRef<WrappedField>::value)
                {
                    order(&field.ref(m_data), f);
                }
                else
                {
                    auto data = field.get(*m_data);
                    order(&data, f);
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
    const ArrayOrder& m_order;
};

} // namespace array_orderer_details

template<typename T>
void order(T* data, const ArrayOrder& order_)
{
    using namespace array_orderer_details;

    if constexpr (IsOptionalV<T>)
    {
        if (data->has_value())
            order(&data->value(), order_);
    }
    else if constexpr (IsVariantV<T>)
    {
        std::visit([&order_](auto& v) { order(&v, order_); }, *data);
    }
    else if constexpr (IsInstrumentedV<T>)
    {
        if (!order_.fields.empty())
            visitAllFields<T>(Orderer<T>{data, order_});
    }
    else if constexpr (IsContainerV<T>)
    {
        if constexpr (IsArrayV<T> || IsSequenceContainerV<T>)
        {
            if (auto c = makeComparator<typename T::value_type>(order_))
            {
                std::stable_sort(data->begin(), data->end(),
                    [&c](const auto& lhs, const auto& rhs) { return c(lhs, rhs) < 0; });
            }
            for (auto& value: *data)
                order(&value, order_);
        }
        else if constexpr (IsSetContainerV<T> || IsUnorderedSetContainerV<T>)
        {
            for (auto& value: *data)
                order(&value, order_);
        }
        else if constexpr (IsAssociativeContainerV<T> || IsUnorderedAssociativeContainerV<T>)
        {
            for (auto& [_, value]: *data)
                order(&value, order_);
        }
    }
}

} // namespace nx::reflect
