// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>

#include <QtCore/QUrlQuery>

namespace nx::network::url {

namespace detail {

NX_NETWORK_API bool convertTo(const std::string& src, std::string* const dst);

template<typename T>
bool convertTo(const std::string& src, std::optional<T>* const dst)
{
    *dst = T();
    return convertTo(src, &dst->value());
}

template<typename T>
bool convertTo(const QString& src, std::optional<T>* const dst)
{
    *dst = T();
    return convertTo(src, &dst->value());
}

} // namespace detail

/**
 * @return false if requested field was not found in urlQuery. true otherwise.
 */
template<typename T>
bool deserializeField(
    const QUrlQuery& urlQuery,
    const std::string& fieldName,
    T* const value)
{
    if (urlQuery.hasQueryItem(QString::fromStdString(fieldName)))
    {
        detail::convertTo(urlQuery.queryItemValue(
            QString::fromStdString(fieldName)).toStdString(), value);
        return true;
    }
    return false;
}

NX_NETWORK_API void serializeField(
    QUrlQuery* const urlQuery,
    const std::string& fieldName,
    const std::string& value);

template<typename T>
void serializeField(
    QUrlQuery* const urlQuery,
    const std::string& fieldName,
    const std::optional<T>& value)
{
    if (value)
        serializeField(urlQuery, fieldName, *value);
}

} // namespace nx::network::url
