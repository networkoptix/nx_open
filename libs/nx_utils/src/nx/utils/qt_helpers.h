// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

namespace nx::utils {

template<typename Container>
auto toQSet(const Container& source)
{
    return QSet(source.begin(), source.end());
}

template<typename Container>
auto toQList(const Container& source)
{
    return QList(source.begin(), source.end());
}

template<typename... Args>
QVariantList makeQVariantList(Args... args)
{
    return QVariantList{{QVariant::fromValue(args)...}};
}

template<typename T>
QList<T> toTypedQList(const QVariantList& source)
{
    QList<T> result;
    result.reserve(source.size());
    for (const QVariant& item: source)
        result.push_back(item.value<T>());
    return result;
}

template<typename Container>
QVariantList toQVariantList(const Container& source)
{
    QVariantList result;
    result.reserve(source.size());
    for (const auto& item: source)
        result.push_back(QVariant::fromValue(item));
    return result;
}

template<typename KeyValueRange /*must map strings to values*/>
QVariantMap toQVariantMap(const KeyValueRange& source)
{
    QVariantMap result;
    for (const auto& [name, value]: source)
    {
        if constexpr (std::is_convertible_v<decltype(name), QString>)
            result.insert(name, QVariant::fromValue(value));
        else
            result.insert(QAnyStringView(name).toString(), QVariant::fromValue(value));
    }

    return result;
}

} // namespace nx::utils
