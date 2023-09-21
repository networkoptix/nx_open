// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <iterator>

namespace nx::utils {

// TODO: Add comments with usage examples.

template<typename BeginType, typename EndType>
class RangeAdapter
{
public:
    using value_type = std::decay_t<decltype(*BeginType())>;

    RangeAdapter(BeginType begin, EndType end): m_begin(begin), m_end(end) {}

    BeginType begin() const { return m_begin; }
    EndType end() const { return m_end; }

private:
    BeginType m_begin;
    EndType m_end;
};

template<typename BeginType, typename EndType>
auto rangeAdapter(BeginType begin, EndType end)
{
    return RangeAdapter<BeginType, EndType>(begin, end);
}

template<typename PairType>
auto rangeAdapter(const PairType& pair)
{
    return rangeAdapter(pair.first, pair.second);
}

template<typename KeyValueContainer>
auto keyValueRange(KeyValueContainer& container)
{
    return rangeAdapter(container.keyValueBegin(), container.keyValueEnd());
}

// Avoid constructing from temporaries.
template<typename KeyValueContainer>
auto keyValueRange(KeyValueContainer&& container) = delete;

template<typename KeyValueContainer>
auto constKeyValueRange(const KeyValueContainer& container)
{
    return rangeAdapter(container.constKeyValueBegin(), container.constKeyValueEnd());
}

template<typename KeyValueContainer>
auto constKeyValueRange(KeyValueContainer& container)
{
    return rangeAdapter(container.constKeyValueBegin(), container.constKeyValueEnd());
}

// Avoid constructing from temporaries.
template<typename KeyValueContainer>
auto constKeyValueRange(KeyValueContainer&& container) = delete;

template<typename KeyValueContainer>
auto keyRange(const KeyValueContainer& container)
{
    return rangeAdapter(container.keyBegin(), container.keyEnd());
}

template<typename KeyValueContainer>
auto keyRange(KeyValueContainer& container)
{
    return rangeAdapter(container.keyBegin(), container.keyEnd());
}

// Avoid constructing from temporaries.
template<typename KeyValueContainer>
auto keyRange(KeyValueContainer&& container) = delete;

template<typename Range>
auto reverseRange(const Range& range)
{
    return rangeAdapter(
        std::make_reverse_iterator(range.end()), std::make_reverse_iterator(range.begin()));
}

} // namespace nx::utils
