#pragma once

#include <string>

#include <boost/optional.hpp>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>

namespace nx {
namespace network {
namespace url {

namespace detail {

NX_NETWORK_API bool convertTo(const QString& src, std::string* const dst);

template<typename T>
bool convertTo(const QString& src, boost::optional<T>* const dst)
{
    *dst = T();
    return convertTo(src, &dst->get());
}

} // namespace detail

/**
 * @return false if requested field was not found in urlQuery. true otherwise.
 */
template<typename T>
bool deserializeField(
    const QUrlQuery& urlQuery,
    const QString& fieldName,
    T* const value)
{
    if (urlQuery.hasQueryItem(fieldName))
    {
        detail::convertTo(urlQuery.queryItemValue(fieldName), value);
        return true;
    }
    return false;
}

NX_NETWORK_API void serializeField(
    QUrlQuery* const urlQuery,
    const QString& fieldName,
    const std::string& value);

template<typename T>
void serializeField(
    QUrlQuery* const urlQuery,
    const QString& fieldName,
    const boost::optional<T>& value)
{
    if (value)
        serializeField(urlQuery, fieldName, value.get());
}

} // namespace url
} // namespace network
} // namespace nx
