// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include "from_string.h"
#include "generic_visitor.h"
#include "type_utils.h"

namespace nx::reflect {

struct Filter
{
    std::string name;
    std::vector<std::string> values;
    std::vector<Filter> fields;
};

/*
 * Recursively removes items from all containers in `data` that do not match filter `values`.
 * Items to remove are called filtered. Any plain value is checked against any of `values` in a
 * filter using Matcher::matches(data, values) function to be not filtered. For objects and
 * associative containers, `name` specifies a field/key name that must be one of `values`. `fields`
 * specifies nested fields of object/container to filter. If a nested container becomes empty as a
 * result of the filtering it is considered as filtered. Associative container values can be
 * filtered in common ignoring their keys using "*" as a filter `name`. "*" field must be the first
 * one in the `fields` to work correctly.
 */
template<typename Matcher, typename T>
bool filter(T* data, const Filter& filter);

namespace filter_details {

template<typename Matcher, typename T>
class Visitor: public GenericVisitor<Visitor<Matcher, T>>
{
public:
    Visitor(T* data, const Filter& filter): m_data{data}, m_filter{filter} {}

    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        if (m_filtered)
            return;

        if (!m_filter.values.empty() && !Matcher::matches(field.get(*m_data), m_filter.values))
        {
            m_filtered = true;
            return;
        }

        auto name = field.name();
        auto next = std::find_if(m_filter.fields.begin(), m_filter.fields.end(),
            [name](auto f) { return f.name == name; });

        if (next == m_filter.fields.end())
            return;

        if constexpr (HasRef<WrappedField>::value)
        {
            m_filtered = filter<Matcher>(&field.ref(m_data), *next);
        }
        else
        {
            auto data{field.get(*m_data)};
            m_filtered = filter<Matcher>(&data, *next);
            if (m_filtered)
                field.set(m_data, std::move(data));
        }
    }

    bool finish() { return m_filtered; }

private:
    template<typename F, typename = void>
    struct HasRef { static constexpr bool value = false; };

    template<typename F>
    struct HasRef<F, detail::void_t<decltype(&F::ref)>> { static constexpr bool value = true; };

private:
    T* m_data;
    const Filter& m_filter;
    bool m_filtered = false;
};

template<typename T>
T keyFromName(const std::string& name)
{
    return fromString<T>(name);
}

} // namespace filter_details

template<typename Matcher, typename T>
bool filter(T* data, const Filter& filter_)
{
    using namespace filter_details;

    if (filter_.values.empty() && filter_.fields.empty())
        return false;

    if (!filter_.values.empty() && !Matcher::matches(*data, filter_.values))
        return true;

    if constexpr (std::is_same_v<std::nullptr_t, T>)
    {
        return true;
    }
    else if constexpr (IsOptionalV<T>)
    {
        return data->has_value() && filter<Matcher>(&data->value(), filter_);
    }
    else if constexpr (IsVariantV<T>)
    {
        bool r = false;
        return std::visit(
            [&r, &filter_](auto& v) { return r || filter<Matcher>(&v, filter_); }, *data);
    }
    else if constexpr (IsInstrumentedV<T>)
    {
        return visitAllFields<T>(filter_details::Visitor<Matcher, T>{data, filter_});
    }
    else if constexpr (IsContainerV<T>)
    {
        if (data->empty())
            return true;

        if constexpr (IsArrayV<T>)
        {
            return std::all_of(data->begin(), data->end(),
                [&filter_](auto& v) { return filter<Matcher>(&v, filter_); });
        }
        else if constexpr (IsSequenceContainerV<T> || IsSetContainerV<T> || IsUnorderedSetContainerV<T>)
        {
            using std::erase_if; //< To use both std and QT erase_if.
            erase_if(*data, [&filter_](auto& v) { return filter<Matcher>(&v, filter_); });
            return data->empty();
        }
        else if constexpr (IsAssociativeContainerV<T> || IsUnorderedAssociativeContainerV<T>)
        {
            for (const auto& field: filter_.fields)
            {
                if (field.name == "*")
                {
                    std::erase_if(
                        *data, [&field](auto& it) { return filter<Matcher>(&it.second, field); });
                    if (data->empty())
                        return true;
                }
                else
                {
                    auto key = keyFromName<typename T::key_type>(field.name);
                    auto it = data->find(key);
                    if (it != data->end() && filter<Matcher>(&it->second, field))
                    {
                        data->erase(it);
                        if (data->empty())
                            return true;
                    }
                }
            }
            return data->empty();
        }
    }
    else
    {
        return false;
    }
}

} // namespace nx::reflect
