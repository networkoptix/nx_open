#pragma once

#include <tuple>
#include <set>
#include <type_traits>

#include <nx/utils/std/optional.h>
#include <nx/utils/general_macros.h>

namespace nx::utils::data_structures {

namespace detail {

template<
    template<typename...> typename Template,
    typename PossibleSpecializationOfTemplate
>
struct IsSpecializationOf_Helper: std::false_type {};

template<template<typename...> typename Template, typename... Args >
struct IsSpecializationOf_Helper<Template, Template<Args...>>:
    std::true_type {};

template<
    template<typename...> typename Template,
    typename PossibleSpecializationOfTemplate
>
constexpr bool isSpecializationOf = IsSpecializationOf_Helper<
    Template,
    PossibleSpecializationOfTemplate>::value;

template<typename T, typename = void>
struct HasMappedType_Helper: public std::false_type {};

template<typename T>
struct HasMappedType_Helper<
    T,
    std::void_t<typename std::remove_reference_t<T>::mapped_type>>: public std::true_type {};

template <typename Template>
constexpr bool hasMappedType = HasMappedType_Helper<Template>::value;

template <typename MaybeTuple>
constexpr bool isTuple = isSpecializationOf<
    std::tuple,
    std::remove_cv_t<std::remove_reference_t<MaybeTuple>>>;

} // namespace detail

//-------------------------------------------------------------------------------------------------

namespace MapHelper
{
    /**
     * Default merge executor. Simply replaces the value with the corresponding one from another
     * map.
     */
    template<typename T>
    class DefaultMergeExecutor
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

    /**
     * Determines if the provided type is a map.
     */
    template<typename MaybeMap>
    constexpr bool isMap()
    {
        return detail::hasMappedType<MaybeMap>;
    }

    template<typename MaybeMap>
    constexpr bool isMap(MaybeMap&& /*maybeMap*/)
    {
        return detail::hasMappedType<MaybeMap>;
    }

    /**
     * Calculates map nesting depth. Depth of a simple flat map equals to 1. Depth of a non map
     * type equals to 0.
     */
    template<typename Map>
    constexpr int mapDepth()
    {
        if constexpr (!detail::hasMappedType<Map>)
            return 0;
        else
            return 1 + mapDepth<typename Map::mapped_type>();
    }

    template<typename Map>
    constexpr int mapDepth(const Map& /*map*/)
    {
        return mapDepth<Map>();
    }

    /**
     * Flattens provided arguments to a single tuple. All arguments that are not an std::tuple
     * remain unchanged, tuples are expanded and its values become members of the result
     * tuple. Tuples are expanded only on the top level, nested tuples remain intact.
     *
     * Example:
     *
     * ```
     * const auto result0 = flatTuple(1, "word"s, std::tuple{"some"s, 2.0, "tuple"s});
     *
     * // Now `result0` is equal to:
     * // std::tuple<int, std::string, std::string, double, std::string> {
     * //     1,
     * //     "word"s,
     * //     "some"s,
     * //     2.0,
     * //     "tuple"s
     * // };
     *
     * const auto result1 = flatTuple(
     *     std::tuple{std::tuple{"string"s, 'c'}, std::tuple{2.0, 1}},
     *     'a',
     *     1.5f);
     *
     * // Now `result0` is equal to :
     * // std::tuple<
     * //     std::tuple<std::string, char>,
     * //     std::tuple<double, int>,
     * //     char,
     * //     float>
     * // {
     * //     std::tuple{"string"s, 'c'},
     * //     std::tuple{2.0, 1},
     * //     'a',
     * //     1.5f,
     * // };
     *
     * ```
     */
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
            if constexpr (detail::isTuple<Head>)
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

    /**
     * Equals to the number of the passed template parameters.
     */
    template<typename... Keys>
    constexpr int keyCount = sizeof...(Keys);

    template<typename...>
    struct Wrapper;

    template<template<typename...> typename Container, typename... Args>
    struct Wrapper<Container<Args...>>
    {
        /**
         * Metafunction returning the map type with `Key` as key type and wrapped map as the value
         * type.
         *
         * Example:
         *
         * ```
         * using Wrapped = MapHelper::Wrapper<std::map<std::string, int>>::wrap<double>;
         * static_assert(std::is_same_v<Wrapped, std::map<double, std::map<std::string, int>>>);
         * ```
         */
        template <typename Key>
        using wrap = Container<Key, Container<Args...>>;
    };
}

//-------------------------------------------------------------------------------------------------

namespace detail {

template<typename NonMap, typename = void>
struct Unwrapped_Helper
{
    using Type = NonMap;
};

template<typename Map>
struct Unwrapped_Helper<Map, std::void_t<typename Map::mapped_type>>
{
    using Type = typename Map::mapped_type;
};

template<typename MaybeMap>
using Unwrapped = typename Unwrapped_Helper<MaybeMap>::Type;

template<typename Map, int level>
struct MappedTypeOnLevel_Helper
{
    using Type = typename MappedTypeOnLevel_Helper<typename Map::mapped_type, level - 1>::Type;
};

template<typename Map>
struct MappedTypeOnLevel_Helper<Map, 0>
{
    using Type = Map;
};

template<typename Map, int level>
using MappedTypeOnLevel = typename MappedTypeOnLevel_Helper<Map, level>::Type;

template<typename MapResultType, typename = void>
struct DeepestMappedType_Helper
{
    using Type = MapResultType;
};

template<typename Map>
struct DeepestMappedType_Helper<Map, std::void_t<typename Map::mapped_type>>
{
    using Type = typename DeepestMappedType_Helper<typename Map::mapped_type>::Type;
};

template<typename Map>
using DeepestMappedType = typename DeepestMappedType_Helper<Map>::Type;

template<
    template<typename, typename> typename Container,
    typename KeysHead,
    typename... KeysAndValue
>
struct NestedMap_Helper
{
    using Type = Container<
        KeysHead,
        typename NestedMap_Helper<Container, KeysAndValue...>::Type>;
};

template<template<typename, typename> typename Container, typename Key, typename Value>
struct NestedMap_Helper<Container, Key, Value>
{
    using Type = Container<Key, Value>;
};

template<template<typename, typename> typename Container, typename... Args>
using NestedMap = typename NestedMap_Helper<Container, Args...>::Type;

template<typename EdgeValue, typename = void>
struct KeyTuple_Helper
{
    using Type = std::tuple<>;
};

template<typename Map>
struct KeyTuple_Helper<Map, std::void_t<typename Map::mapped_type>>
{
    using Type = decltype(std::tuple_cat(
        std::declval<std::tuple<typename Map::key_type>>(),
        std::declval<typename KeyTuple_Helper<typename Map::mapped_type>::Type>()));
};

template<typename Map>
using KeyTuple = typename KeyTuple_Helper<Map>::Type;

template<typename Map>
using MapKeys = std::set<KeyTuple<Map>>;

template<typename Map, typename CurrentNestedMap, typename... Keys>
MapKeys<Map> keys(
    MapKeys<Map>& result,
    const CurrentNestedMap& currentNestedMap,
    const Keys&... mapKeys)
{
    if constexpr (MapHelper::mapDepth<Map>() == sizeof...(Keys))
    {
        result.insert(std::make_tuple(mapKeys...));
    }
    else
    {
        for (const auto& [key, nestedMap]: currentNestedMap)
            keys<Map>(result, nestedMap, mapKeys..., key);
    }

    return result;
}

template<typename Map, typename Function, typename... Keys>
void forEach(const Map& nestedMap, Function func, const Keys&... keys)
{
    if constexpr (MapHelper::mapDepth<Map>() == 1)
    {
        for (const auto& [key, value]: nestedMap)
            func(std::make_tuple(keys..., key), value);
    }
    else
    {
        for (const auto& [key, value]: nestedMap)
            detail::forEach(value, func, keys..., key);
    }
}

} // namespace detail

//-------------------------------------------------------------------------------------------------

namespace MapHelper
{
    /**
     * Helper for declaring nested maps. First template parameter is a container type, other
     * parameters are series of key and value types.
     */
    template<template<typename, typename> typename Container, typename... KeysAndValueTypes>
    using NestedMap = detail::NestedMap<Container, KeysAndValueTypes...>;

    /**
     * Metafunction returning mapped type on the provided level of nesting. Level 0 corresponds to
     * the map itself.
     *
     * Example:
     *
     * ```
     * using StringIntMap = std::map<std::string, int>;
     * using StringStringIntMap = std::map<std::string, StringIntMap>;
     *
     * static_assert(std::is_same_v<MappedTypeOnLevel<StringStringIntMap, 0>, StringStringIntMap>);
     * static_assert(std::is_same_v<MappedTypeOnLevel<StringStringIntMap, 1>, StringIntMap);
     * static_assert(std::is_same_v<MappedTypeOnLevel<StringStringIntMap, 2>, int);
     * ```
     */
    template<typename Map, int level>
    using MappedTypeOnLevel = detail::MappedTypeOnLevel<Map, level>;

    /**
     * Metafunction providing the mapped type on the deepest level of nesting.
     *
     * Example:
     *
     * ```
     * using StringIntMap = std::map<std::string, int>;
     * using StringStringIntMap = std::map<std::string, StringIntMap>;
     * static_assert(std::is_same_v<DeepestMappedType<StringStringIntMap>, int>);
     * ```
     */
    template<typename Map>
    using DeepestMappedType = detail::DeepestMappedType<Map>;

    /**
     * Removes one level of nesting from the provided type.
     *
     * Example:
     *
     * ```
     * using StringIntMap = std::map<std::string, int>;
     * using StringStringIntMap = std::map<std::string, StringIntMap>;
     * static_assert(std::is_same_v<Unwrapped<StringStringIntMap>, StringIntMap>);
     * ```
     */
    template<typename Map>
    using Unwrapped = detail::Unwrapped<Map>;

    /**
     * Helper providing the type of the tuple containing key chains for the provided map.
     *
     * Example:
     *
     * ```
     * using StringIntMap = std::map<std::string, int>;
     * using StringStringIntMap = std::map<std::string, StringIntMap>;
     * static_assert(std::is_same_v<
     *     KeyTuple<StringStringIntMap>,
     *     std::tuple<std::string, std::string>>);
     *
     * static_assert(std::is_same_v<KeyTuple<StringIntMap>, std::tuple<std::string>>);
     * ```
     */
    template<typename Map>
    using KeyTuple = detail::KeyTuple<Map>;

    /**
     * Container for Map key chains.
     */
    template<typename Map>
    using MapKeys = detail::MapKeys<Map>;

    /**
     * @return A set of all the keys present in the provided map.
     *
     * Example:
     *
     * ```
     * MapHelper::NestedMap<std::map, std::string, std::string, int> nestedMap = {
     *     {"some", {{"value", 10}}},
     *     {"other" {{"value0", 20}, {"value1", 30}}}
     * };
     *
     * auto mapKeys = keys(nestedMap);
     * // Now `mapKeys` is:
     * // std::set<std::tuple<std::string, std::string>> {
     * //     {"some", "value"},
     * //     {"other", "value0"},
     * //     {"other", "value1"}
     * // }
     * ```
     */
    template<typename Map>
    MapKeys<Map> keys(const Map& map)
    {
        MapKeys<Map> result;
        detail::keys<Map>(result, map);
        return result;
    }

    /**
     * Sets a map value. Can set a value on any level of nesting.
     *
     * @return Reference to the value that has been set.
     *
     * Example:
     *
     * ```
     * MapHelper::NestedMap<std::map, std::string, std::string, int> nestedMap;
     * MapHelper::set(&nestedMap, "some", "value", 10);
     *
     * std::map<std::string, int> flatMap{{"value0", 20}, {"value1", 30}};
     * MapHelper::set(&nestedMap, "other", flatMap);
     *
     * // Result:
     * // {
     * //     {"some", {{"value", 10}}},
     * //     {"other" {{"value0", 20}, {"value1", 30}}}
     * // }
     * ```
     */
    template<typename Map, typename FirstKey, typename... KeysAndValue>
    auto set(Map* inOutNestedMap, FirstKey&& firstKey, KeysAndValue&&... keysAndValue)
        -> MappedTypeOnLevel<Map, keyCount<FirstKey, KeysAndValue...> - /*value*/ 1>&
    {
        if constexpr (sizeof...(KeysAndValue) == 0) //< Value is in the `firstKey` parameter.
        {
            *inOutNestedMap = firstKey;
            return *inOutNestedMap;
        }
        else if constexpr (sizeof...(KeysAndValue) == 1) //< Only a value is in the parameter pack.
        {
            auto result = inOutNestedMap->insert_or_assign(
                std::forward<FirstKey>(firstKey),
                std::forward<KeysAndValue>(keysAndValue)...);

            auto entryIterator = result.first;
            return entryIterator->second;
        }
        else
        {
            return set(&(*inOutNestedMap)[std::forward<FirstKey>(firstKey)],
                std::forward<KeysAndValue>(keysAndValue)...);
        }
    };

    /**
     * @return Pointer to the value corresponding to the specified keys, nullptr if such a value
     * does not exist. Value can be retrieved from any level of nesting.
     *
     * Example:
     *
     * ```
     * MapHelper::NestedMap<std::map, std::string, std::string, int> nestedMap = {
     *     {"some", {{"value", 10}}},
     *     {"other" {{"value0", 20}, {"value1", 30}}}
     * };
     *
     * // After the next line `result` is pointing to `nestedMap["some"]["value"]`.
     * auto result = MapHelper::getPointer(nestedMap, "some", "value");
     *
     * // After the line below `result == nullptr`.
     * auto result = MapHelper::getPointer(nestedMap, "anything", "that doesn't exist in the map");
     *
     * // `anotherResult` is pointing to `nestedMap["other"]` map.
     * auto anotherResult = MapHelper::getPointer(nestedMap, "other");
     * ```
     */
    template<typename Map>
    Map* getPointer(Map& nestedMap) { return &nestedMap; }

    template<typename Map>
    const Map* getPointer(const Map& nestedMap) { return &nestedMap; }

    template<typename Map, typename FirstKey, typename... RemainingKeys>
    const MappedTypeOnLevel<Map, keyCount<FirstKey, RemainingKeys...>>* getPointer(
        const Map& nestedMap,
        const FirstKey& firstKey,
        const RemainingKeys&... remainingKeys)
    {
        const auto itr = nestedMap.find(firstKey);
        if (itr == nestedMap.cend())
            return nullptr;

        if constexpr (sizeof...(RemainingKeys) == 0)
            return &itr->second;
        else
            return getPointer(itr->second, remainingKeys...);
    };

    template<typename Map, typename FirstKey, typename... RemainingKeys>
    MappedTypeOnLevel<Map, keyCount<FirstKey, RemainingKeys...>>* getPointer(
        Map& nestedMap,
        const FirstKey& firstKey,
        const RemainingKeys&... remainingKeys)
    {
        return const_cast<MappedTypeOnLevel<Map, keyCount<FirstKey, RemainingKeys...>>*>(
            getPointer(const_cast<const Map&>(nestedMap), firstKey, remainingKeys...));
    };

    template<typename Map>
    Map& get(Map& nestedMap) { return *getPointer(nestedMap); }

    template<typename Map, typename FirstKey, typename... RemainingKeys>
    MappedTypeOnLevel<Map, keyCount<FirstKey, RemainingKeys...>>& get(
        Map& nestedMap,
        const FirstKey& firstKey,
        const RemainingKeys&... remainingKeys)
    {
        auto result = getPointer(nestedMap, firstKey, remainingKeys...);
        if (result)
            return *result;

        return set(
            &nestedMap,
            firstKey,
            remainingKeys...,
            MappedTypeOnLevel<Map, keyCount<FirstKey, RemainingKeys...>>());
    }

    /**
     * @return A reference to the value corresponding to the specified keys. If such a value is
     * absent in the map throws the `std::out_of_range` exception.
     *
     * Example:
     *
     * ```
     * MapHelper::NestedMap<std::map, std::string, std::string, int> nestedMap = {
     *     {"some", {{"value", 10}}},
     *     {"other" {{"value0", 20}, {"value1", 30}}}
     * };
     *
     * // After this line `result0` is a reference to `nestedMap["some"]["value"]`.
     * auto& result0 = MapHelper::getPointer(nestedMap, "some", "value");
     *
     * // If uncommented, the statement below throws the `std::out_of_range` exception.
     * // auto result1 =
     * //   MapHelper::getPointer(nestedMap, "anything", "that doesn't exist in the map");
     *
     * // After the line below `result2` is a reference to `nestedMap["other"]` map.
     * auto& result2 = MapHelper::getPointer(nestedMap, "other");
     * ```
     */
    template<typename Map>
    Map& getOrThrow(Map& nestedMap) { return *getPointer(nestedMap); }

    template<typename Map>
    const Map& getOrThrow(const Map& nestedMap) { return *getPointer(nestedMap); }

    template<typename Map, typename FirstKey, typename... RemainingKeys>
    const MappedTypeOnLevel<Map, keyCount<FirstKey, RemainingKeys...>>& getOrThrow(
        const Map& nestedMap,
        const FirstKey& firstKey,
        const RemainingKeys&... remainingKeys)
    {
        const auto result = getPointer(nestedMap, firstKey, remainingKeys...);
        if (!result)
            throw std::out_of_range("Key chain doesn't exist");

        return *result;
    };

    template<typename Map, typename FirstKey, typename... RemainingKeys>
    MappedTypeOnLevel<Map, keyCount<FirstKey, RemainingKeys...>>& getOrThrow(
        Map& nestedMap,
        const FirstKey& firstKey,
        const RemainingKeys&... remainingKeys)
    {
        auto result = getPointer(nestedMap, firstKey, remainingKeys...);
        if (!result)
            throw std::out_of_range("Key chain doesn't exist");

        return *result;
    };

    /**
     * @return An std::optional wrapped value corresponding to the specified keys or `std::nullopt`
     * in the case of the value absence. Take a note this method makes a copy of existing value.
     */
    template<typename Map>
    std::optional<Map> getOptional(const Map& nestedMap) { return *getPointer(nestedMap); }

    template<typename Map, typename FirstKey, typename... RemainingKeys>
    std::optional<MappedTypeOnLevel<Map, keyCount<FirstKey, RemainingKeys...>>> getOptional(
        const Map& nestedMap,
        const FirstKey& firstKey,
        const RemainingKeys&... remainingKeys)
    {
        auto result = getPointer(nestedMap, firstKey, remainingKeys...);
        if (!result)
            return std::nullopt;

        return *result;
    };

    /**
     * @return true if provided nested map contains the value corresponding to the provided keys,
     * false otherwise.
     *
     * Example:
     *
     * ```
     * MapHelper::NestedMap<std::map, std::string, std::string, int> nestedMap = {
     *     {"some", {{"value", 10}}},
     *     {"other" {{"value0", 20}, {"value1", 30}}}
     * };
     *
     * MapHelper::contains(nestedMap, "some"); //< true
     * MapHelper::contains(nestedMap, "some", "value"); //< true
     * MapHelper::contains(nestedMap, "does not exists"); //< false
     * MapHelper::contains(nestedMap, "other", "value2"); //< false
     *
     * ```
     */
    template<typename Map, typename FirstKey, typename... RemainingKeys>
    bool contains(
        const Map& nestedMap,
        const FirstKey& firstKey,
        const RemainingKeys&... remainingKeys)
    {
        if (getPointer(nestedMap, firstKey, remainingKeys...))
            return true;

        return false;
    }

    /**
     * Removes the value corresponding to the specified keys from the map. If the outer map,
     * containing value becomes empty after the value removal it is also removed.
     *
     * Example:
     *
     * ```
     * MapHelper::NestedMap<std::map, std::string, std::string, int> nestedMap = {
     *     {"some", {{"value", 10}}},
     *     {"other" {{"value0", 20}, {"value1", 30}}}
     *     {"another", {"value2", 40}}
     * };
     *
     * MapHelper::erase(nestedMap, "some", "value");
     * // Now `nestedMap` is:
     * // {
     * //     {"other" {{"value0", 20}, {"value1", 30}}}
     * //     {"another", {"value2", 40}}
     * // }
     *
     * MapHelper::erase(nestedMap, "other", "value1");
     * MapHelper::erase(nestedMap, "another");
     *
     * // Now `nestedMap` is:
     * // {
     * //     {"other" {{"value0", 20}}}
     * // }
     * ```
     */
    template<typename Map, typename FirstKey, typename... RemainingKeys>
    void erase(
        Map* inOutNestedMap,
        const FirstKey& firstKey,
        const RemainingKeys&... remainingKeys)
    {
        const auto itr = inOutNestedMap->find(firstKey);
        if (itr == inOutNestedMap->cend())
            return;

        if constexpr (sizeof...(RemainingKeys) == 0)
        {
            inOutNestedMap->erase(firstKey);
        }
        else
        {
            erase(&itr->second, remainingKeys...);
            if (itr->second.empty())
                inOutNestedMap->erase(itr);
        }
    }

    /**
     * Merges two map.
     * @param mergeExecutor Functor providing merging logic. Must be a Callable with the following
     *     signature: `std::optional(const DeepestMapType<Map>*, const DeepestMapType<Map>*)`.
     *
     * @return Merged map
     */
    template<
        typename Map,
        typename MergeExecutor = DefaultMergeExecutor<DeepestMappedType<Map>>
    >
    Map merge(const Map& first, const Map& second, MergeExecutor&& mergeExecutor = MergeExecutor())
    {
        Map result;
        const auto firstKeys = keys(first);
        const auto secondKeys = keys(second);

        MapKeys<Map> unitedKeys;
        std::set_union(
            firstKeys.begin(), firstKeys.end(),
            secondKeys.begin(), secondKeys.end(),
            std::inserter(unitedKeys, unitedKeys.begin()));

        for (const auto& key: unitedKeys)
        {
            auto merged = mergeExecutor(
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(getPointer), flatTuple(first, key)),
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(getPointer), flatTuple(second, key)));

            if (merged)
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(set), flatTuple(&result, key, *merged));
        }

        return result;
    }

    /**
     * Merges two maps in-place (in the map provided in the `first` argument).
     */
    template<
        typename Map,
        typename MergeExecutor = DefaultMergeExecutor<DeepestMappedType<Map>>
    >
    void merge(Map* first, const Map& second, const MergeExecutor& mergeExecutor = MergeExecutor())
    {
        const auto firstKeys = keys(*first);
        const auto secondKeys = keys(second);

        MapKeys<Map> unitedKeys;
        std::set_union(
            firstKeys.begin(), firstKeys.end(),
            secondKeys.begin(), secondKeys.end(),
            std::inserter(unitedKeys, unitedKeys.begin()));

        for (const auto& key: unitedKeys)
        {
            auto merged = mergeExecutor(
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(getPointer), flatTuple(*first, key)),
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(getPointer), flatTuple(second, key)));

            if (merged)
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(set), flatTuple(first, key, *merged));
        }
    }

    /**
     * Executes the functor for each value in the map. If additional keys are provided only the
     * entries corresponding to that keys are passed to the functor.
     *
     * @param func The functor to execute on each map value. Must have the following signature:
     *     void(const std::tuple<Keys...>& keys, const Value& value) where `keys` is a tuple of
     *     keys to access the value.
     *
     * Example:
     *
     * ```
     * MapHelper::NestedMap<std::map, std::string, std::string, int> nestedMap = {
     *     {"some", {{"value", 10}}},
     *     {"other" {{"value0", 20}, {"value1", 30}}}
     *     {"another", {"value2", 40}}
     * };
     *
     * auto printValue =
     *     [](const std::tuple<std::string, std::string>& keys, int value)
     *     {
     *          std::cout << keys.get<0>() << " " << keys.get<1>() << ": " << value << std::endl;
     *     }
     *
     * MapHelper::forEach(nestedMap, printValue);
     *
     * // Outputs the following:
     * // some value: 10
     * // other value0: 20
     * // other value1: 30
     * // another value2: 40
     *
     * auto printNestedValue =
     *     [](const std::tuple<std::string>& keys, int value)
     *     {
     *          std::cout << keys.get<0>() << ": " << value << std::endl;
     *     }
     *
     * MapHelper::forEach(nestedMap, printValue, "other")
     *
     * // Outputs the following:
     * // value0: 20
     * // value1: 30
     *```
     */
    template<typename Map, typename Function, typename... Keys>
    void forEach(const Map& nestedMap, Function func, const Keys&... keys)
    {
        if constexpr (sizeof...(Keys) == 0)
        {
            detail::forEach(nestedMap, func);
        }
        else
        {
            if (contains(nestedMap, keys...))
                detail::forEach(getOrThrow(nestedMap, keys...), func);
        }
    }

    /**
     * @param filter Functor providing the filtering logic. Must have the following signature:
     *     bool(const DeepestMappedType<Map>& value)
     * @return Map filtered by the logic provided in the `filter` functor.
     */
    template<typename Map, typename FilterFunc>
    Map filtered(const Map& nestedMap, FilterFunc&& filter)
    {
        Map result;
        auto filterFunc =
            [&result, &filter](const auto& keysTuple, const auto& value)
            {
                if (filter(keysTuple, value))
                {
                    std::apply(
                        NX_WRAP_FUNC_TO_LAMBDA(MapHelper::set),
                        MapHelper::flatTuple(&result, keysTuple, value));
                }
            };

        MapHelper::forEach(nestedMap, filterFunc);
        return result;
    }

    /**
     * Removes from the first map all elements that have no corresponding elements in the second
     * map. Other elements are merged in-place with the help of the given merge executor.
     */
    template<
        typename Map,
        typename MergeExecutor = DefaultMergeExecutor<DeepestMappedType<Map>>
    >
    void intersected(
        Map* first,
        const Map& second,
        const MergeExecutor& mergeExecutor = MergeExecutor())
    {
        const auto firstKeys = keys(*first);
        const auto secondKeys = keys(second);

        MapKeys<Map> intersectedKeys;
        std::set_intersection(
            firstKeys.begin(), firstKeys.end(),
            secondKeys.begin(), secondKeys.end(),
            std::inserter(intersectedKeys, intersectedKeys.begin()));

        for (const auto& key: firstKeys)
        {
            if (intersectedKeys.find(key) == intersectedKeys.cend())
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(erase), flatTuple(first, key));
        }

        for (const auto& key: intersectedKeys)
        {
            auto merged = mergeExecutor(
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(getPointer), flatTuple(*first, key)),
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(getPointer), flatTuple(second, key)));

            if (merged)
                std::apply(NX_WRAP_FUNC_TO_LAMBDA(set), flatTuple(first, key, *merged));
        }
    }
}

} // namespace nx::utils::data_structures
