#pragma once

#include <tuple>
#include <type_traits>

#include <nx/utils/std/optional.h>

namespace nx::utils::data_structures {

namespace MapHelper
{
    template<typename T>
    constexpr bool isMap()
    {
        return data_structures::detail::hasMappedType<T>::value;
    }

    template<typename T>
    constexpr bool isMap(T&& maybeMap)
    {
        return data_structures::detail::hasMappedType<T>::value;
    }

    template<typename Map>
    constexpr int mapDepth()
    {
        if constexpr (!data_structures::detail::hasMappedType<Map>::value)
            return 0;
        else
            return 1 + mapDepth<typename Map::mapped_type>();
    }

    template<typename Map>
    constexpr int mapDepth(const Map& map)
    {
        return mapDepth<Map>();
    }

    template<typename... Keys>
    constexpr int keysNumber = sizeof...(Keys);

    template<typename...>
    struct Wrapper;

    template<template<typename...> typename Container, typename... Args>
    struct Wrapper<Container<Args...>>
    {
        template <typename Key>
        using wrap = Container<Key, Container<Args...>>;
    };
}

namespace detail {

template<template<typename...> typename Template, typename T>
struct IsSpecializationOf: std::false_type {};

template<template<typename...> typename Template, typename... Args >
struct IsSpecializationOf<Template, Template<Args...>>: std::true_type {};

template<typename T, typename = void>
struct hasMappedType: public std::false_type {};

template<typename T>
struct hasMappedType<
    T,
    std::void_t<typename std::remove_reference_t<T>::mapped_type>>: public std::true_type {};

template<typename NonMap, typename = void>
struct UnwrappedInternal
{
    using type = NonMap;
};

template<typename Map>
struct UnwrappedInternal<Map, std::void_t<typename Map::mapped_type>>
{
    using type = typename Map::mapped_type;
};

template<typename Map, int level>
struct MappedTypeOnLevelInternal
{
    using type = typename MappedTypeOnLevelInternal<typename Map::mapped_type, level - 1>::type;
};

template<typename Map>
struct MappedTypeOnLevelInternal<Map, 0>
{
    using type = Map;
};

template<typename MapResultType, typename = void>
struct DeepestMappedTypeInternal
{
    using type = MapResultType;
};

template<typename Map>
struct DeepestMappedTypeInternal<Map, std::void_t<typename Map::mapped_type>>
{
    using type = typename DeepestMappedTypeInternal<typename Map::mapped_type>::type;
};

template<
    template<typename, typename> typename Container,
    typename KeysHead,
    typename... KeysAndValue>
struct NestedMapInternal
{
    using type = Container<
        KeysHead,
        typename NestedMapInternal<Container, KeysAndValue...>::type>;
};

template<template<typename, typename> typename Container, typename Key, typename Value>
struct NestedMapInternal<Container, Key, Value>
{
    using type = Container<Key, Value>;
};

template<typename T>
class DefaultMerger
{
public:
    std::optional<T> operator()(const T* first, const T* second) const
    {
        if (second)
            return *second;
        if (first)
            return *first;

        return std::nullopt;
    }
};

template<typename EdgeValue, typename = void>
struct KeyTupleInternal
{
    using type = std::tuple<>;
};

template<typename Map>
struct KeyTupleInternal<Map, std::void_t<typename Map::mapped_type>>
{
    using type = decltype(std::tuple_cat(
        std::declval<std::tuple<typename Map::key_type>>(),
        std::declval<typename KeyTupleInternal<typename Map::mapped_type>::type>()));
};

template<typename Map, typename CurrentNestedMap, typename... Keys>
std::set<typename KeyTupleInternal<Map>::type> keysInternal(
    std::set<typename KeyTupleInternal<Map>::type>& result,
    const CurrentNestedMap& currentNestedMap,
    const Keys&... keys)
{
    if constexpr (std::tuple_size<typename KeyTupleInternal<Map>::type>() == sizeof...(keys))
    {
        result.insert(std::make_tuple(keys...));
    }
    else
    {
        for (const auto&[key, nestedMap] : currentNestedMap)
            keysInternal<Map>(result, nestedMap, keys..., key);
    }

    return result;
}

template<typename Map, typename Function, typename... Keys>
void forEachInternal(const Map& nestedMap, Function func, const Keys&... keys)
{
    if constexpr (MapHelper::mapDepth<Map>() == 1)
    {
        for (const auto&[key, value]: nestedMap)
            func(std::make_tuple(keys..., key), value);
    }
    else
    {
        for (const auto&[key, value]: nestedMap)
            forEachInternal(value, func, keys..., key);
    }
}

} // namespace detail

//-------------------------------------------------------------------------------------------------

namespace MapHelper
{
    template<typename Map, int level>
    using MappedTypeOnLevel =
        typename data_structures::detail::MappedTypeOnLevelInternal<Map, level>::type;

    template<typename Map>
    using DeepestMappedType =
        typename data_structures::detail::DeepestMappedTypeInternal<Map>::type;

    template<template<typename, typename> typename Container, typename... Ts>
    using NestedMap =
        typename data_structures::detail::NestedMapInternal<Container, Ts...>::type;

    template<typename Map>
    using Unwrapped = typename data_structures::detail::UnwrappedInternal<Map>::type;

    template<typename... ArgsAndTuple>
    auto flatTuple(ArgsAndTuple&&...)
    {
        return std::tuple<>();
    };

    template<typename Head, typename... Tail>
    auto flatTuple(Head&& head, Tail&&... tail)
    {
        if constexpr (sizeof...(Tail) == 0)
        {
            constexpr bool isTuple = data_structures::detail::IsSpecializationOf<
                std::tuple,
                std::remove_cv_t<std::remove_reference_t<Head>>>::value;

            if constexpr (isTuple)
                return head;
            else
                return std::make_tuple(std::forward<Head>(head));
        }
        else
        {
            return std::tuple_cat(
                flatTuple(std::forward<Head>(head)),
                flatTuple(std::forward<Tail>(tail)...));
        }
    };

    template<typename Map, typename FirstKey, typename... KeysAndValue>
    void set(Map* inOutNestedMap, FirstKey&& firstKey, KeysAndValue&&... keysAndValue)
    {
        if constexpr (sizeof...(KeysAndValue) == 0)
        {
            *inOutNestedMap = firstKey;
        }
        else if constexpr (sizeof...(KeysAndValue) == 1) //< Only a value is in the parameter pack.
        {
            inOutNestedMap->insert_or_assign(
                std::forward<FirstKey>(firstKey),
                std::forward<KeysAndValue>(keysAndValue)...);
        }
        else
        {
            set(&(*inOutNestedMap)[std::forward<FirstKey>(firstKey)],
                std::forward<KeysAndValue>(keysAndValue)...);
        }
    };

    template<typename Map>
    Map& get(Map& nestedMap) { return nestedMap; }

    template<typename Map, typename FirstKey, typename...KeysRest>
    MappedTypeOnLevel<Map, keysNumber<FirstKey, KeysRest...>>& get(
        Map& nestedMap,
        const FirstKey& firstKey,
        const KeysRest&... keysRest)
    {
        if constexpr (sizeof...(KeysRest) == 0)
            return nestedMap[firstKey];
        else
            return get(nestedMap[firstKey], keysRest...);
    }

    template<typename Map>
    Map& getOrThrow(Map& nestedMap) { return nestedMap; }

    template<typename Map>
    const Map& getOrThrow(const Map& nestedMap) { return nestedMap; }

    template<typename Map, typename FirstKey, typename... KeysRest>
    const MappedTypeOnLevel<Map, keysNumber<FirstKey, KeysRest...>>& getOrThrow(
        const Map& nestedMap,
        const FirstKey& firstKey,
        const KeysRest&... keysRest)
    {
        if constexpr (sizeof...(KeysRest) == 0)
            return nestedMap.at(firstKey);
        else
            return getOrThrow(nestedMap.at(firstKey), keysRest...);
    };

    template<typename Map, typename FirstKey, typename... KeysRest>
    MappedTypeOnLevel<Map, keysNumber<FirstKey, KeysRest...>>& getOrThrow(
        Map& nestedMap,
        const FirstKey& firstKey,
        const KeysRest&... keysRest)
    {
        return const_cast<MappedTypeOnLevel<Map, keysNumber<FirstKey, KeysRest...>>&>(
            getOrThrow(const_cast<const Map&>(nestedMap), firstKey, keysRest...));
    };

    template<typename Map>
    Map* getPointer(Map& nestedMap) { return &nestedMap; }

    template<typename Map>
    const Map* getPointer(const Map& nestedMap) { return &nestedMap; }

    template<typename Map, typename FirstKey, typename... KeysRest>
    const MappedTypeOnLevel<Map, keysNumber<FirstKey, KeysRest...>>* getPointer(
        const Map& nestedMap,
        const FirstKey& firstKey,
        const KeysRest&... keysRest)
    {
        const auto itr = nestedMap.find(firstKey);
        if (itr == nestedMap.cend())
            return nullptr;

        if constexpr (sizeof...(KeysRest) == 0)
            return &itr->second;
        else
            return getPointer(itr->second, keysRest...);
    };

    template<typename Map, typename FirstKey, typename... KeysRest>
    MappedTypeOnLevel<Map, keysNumber<FirstKey, KeysRest...>>* getPointer(
        Map& nestedMap,
        const FirstKey& firstKey,
        const KeysRest&... keysRest)
    {
        return const_cast<MappedTypeOnLevel<Map, keysNumber<FirstKey, KeysRest...>>*>(
            getPointer(const_cast<const Map&>(nestedMap), firstKey, keysRest...));
    };

    template<typename Map>
    std::optional<Map> getOrNullopt(const Map& nestedMap) { return nestedMap; }

    template<typename Map, typename FirstKey, typename... KeysRest>
    std::optional<MappedTypeOnLevel<Map, keysNumber<FirstKey, KeysRest...>>> getOrNullopt(
        const Map& nestedMap,
        const FirstKey& firstKey,
        const KeysRest&... keysRest)
    {
        const auto itr = nestedMap.find(firstKey);
        if (itr == nestedMap.cend())
            return std::nullopt;

        if constexpr (sizeof...(KeysRest) == 0)
            return itr->second;
        else
            return getOrNullopt(itr->second, keysRest...);
    };

    template<typename Map, typename FirstKey, typename... KeysRest>
    bool contains(const Map& nestedMap, const FirstKey& firstKey, const KeysRest&... keysRest)
    {
        const auto itr = nestedMap.find(firstKey);
        if (itr == nestedMap.cend())
            return false;

        if constexpr (sizeof...(KeysRest) == 0)
            return true;
        else
            return contains(itr->second, keysRest...);
    }

    template<typename Map, typename FirstKey, typename... KeysRest>
    void erase(Map* inOutNestedMap, const FirstKey& firstKey, const KeysRest&... keysRest)
    {
        const auto itr = inOutNestedMap->find(firstKey);
        if (itr == inOutNestedMap->cend())
            return;

        if constexpr (sizeof...(keysRest) == 0)
        {
            inOutNestedMap->erase(firstKey);
        }
        else
        {
            erase(&itr->second, keysRest...);
            if (itr->second.empty())
                inOutNestedMap->erase(itr);
        }
    }

    template<typename Map>
    using KeyTuple = typename data_structures::detail::KeyTupleInternal<Map>::type;

    // We can't pass function templates as template parameters, so we're unable to use them with
    // std::apply, std::invoke etc. But generic lambdas are ok in such contexts (because they are
    // essentially class templates).
    #define NX_WRAP_FUNC_TO_LAMBDA(FUNC) \
            [](auto&&... args) { return FUNC(std::forward<decltype(args)>(args)...); }

    template<typename Map>
    std::set<KeyTuple<Map>> keys(const Map& map)
    {
        std::set<KeyTuple<Map>> result;
        data_structures::detail::keysInternal<Map>(result, map);
        return result;
    }

    template<
        typename Map,
        typename Merger = data_structures::detail::DefaultMerger<DeepestMappedType<Map>>>
    Map merge(const Map& first, const Map& second, Merger&& merger = Merger())
    {
        Map result;
        const auto firstKeys = keys(first);
        const auto secondKeys = keys(second);

        std::set<KeyTuple<Map>> unitedKeys;
        std::set_union(
            firstKeys.begin(), firstKeys.end(),
            secondKeys.begin(), secondKeys.end(),
            std::inserter(unitedKeys, unitedKeys.begin()));

        for (const auto& key: unitedKeys)
        {
            auto merged = merger(
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(getPointer), flatTuple(first, key)),
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(getPointer), flatTuple(second, key)));

            if (merged)
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(set), flatTuple(&result, key, *merged));
        }

        return result;
    }

    template<
        typename Map,
        typename Merger = data_structures::detail::DefaultMerger<DeepestMappedType<Map>>>
    void merge(Map* first, const Map& second, Merger&& merger = Merger())
    {
        const auto firstKeys = keys(*first);
        const auto secondKeys = keys(second);

        std::set<KeyTuple<Map>> unitedKeys;
        std::set_union(
            firstKeys.begin(), firstKeys.end(),
            secondKeys.begin(), secondKeys.end(),
            std::inserter(unitedKeys, unitedKeys.begin()));

        for (const auto& key: unitedKeys)
        {
            auto merged = merger(
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(getPointer), flatTuple(*first, key)),
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(getPointer), flatTuple(second, key)));

            if (merged)
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(set), flatTuple(first, key, *merged));
        }
    }

    template<typename Map, typename Function, typename... Keys>
    void forEach(const Map& nestedMap, Function func, const Keys&... keys)
    {
        if constexpr (sizeof...(Keys) == 0)
        {
            data_structures::detail::forEachInternal(nestedMap, func);
        }
        else
        {
            if (contains(nestedMap, keys...))
                data_structures::detail::forEachInternal(getOrThrow(nestedMap, keys...), func);
        }
    }

    template<typename Map, typename FilterFunc>
    Map filtered(const Map& nestedMap, FilterFunc&& filter)
    {
        Map result;
        auto doFilter =
            [&result, &filter](const auto& keysTuple, const auto& value)
            {
                if (filter(keysTuple, value))
                {
                    std::apply(
                        NX_WRAP_FUNC_TO_LAMBDA(MapHelper::set),
                        MapHelper::flatTuple(&result, keysTuple, value));
                }
            };

        MapHelper::forEach(nestedMap, doFilter);
        return result;
    }
}

} // namespace nx::utils::data_structures
