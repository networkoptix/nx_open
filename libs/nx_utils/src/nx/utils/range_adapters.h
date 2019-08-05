#pragma once

#include <iterator>

namespace nx::utils {

// TODO: Add comments with usage examples.

template<typename BeginType, typename EndType>
class RangeAdapter
{
public:
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
auto keyValueRange(KeyValueContainer&& container)
{
    return rangeAdapter(container.keyValueBegin(), container.keyValueEnd());
}

template<typename KeyValueContainer>
auto constKeyValueRange(KeyValueContainer&& container)
{
    return rangeAdapter(container.constKeyValueBegin(), container.constKeyValueEnd());
}

template<typename Range>
auto reverseRange(const Range& range)
{
    return rangeAdapter(
        std::make_reverse_iterator(range.end()), std::make_reverse_iterator(range.begin()));
}

} // namespace nx::utils
