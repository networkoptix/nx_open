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

#if __cpp_concepts >= 201907L
    template<typename T>
    concept HasKeyValueBeginEnd = requires(T&& t)
    {
        t.keyValueBegin();
        t.keyValueEnd();
    };

    template<typename T>
    concept HasBeginEnd = requires(T&& t)
    {
        t.begin();
        t.end();
    };
#else
    template<typename, typename = void>
    constexpr bool HasKeyValueBeginEnd = false;

    template<typename T>
    constexpr bool HasKeyValueBeginEnd<T,
        std::void_t<
            decltype(std::declval<T>().keyValueBegin()),
            decltype(std::declval<T>().keyValueEnd())>> = true;

    template<typename, typename = void>
    constexpr bool HasBeginEnd = false;

    template<typename T>
    constexpr bool HasBeginEnd<T,
        std::void_t<
            decltype(std::declval<T>().begin()),
            decltype(std::declval<T>().end())>> = true;
#endif

template<typename Container>
auto keyValueRange(Container& container)
{
    constexpr bool hasKVbe = HasKeyValueBeginEnd<Container>;
    constexpr bool hasBe = HasBeginEnd<Container>;
    if constexpr(hasKVbe)
        return rangeAdapter(container.keyValueBegin(), container.keyValueEnd());
    else if constexpr(hasBe)
        return rangeAdapter(container.begin(), container.end());
    else
        static_assert(hasKVbe || hasBe, "Container not supported");
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
