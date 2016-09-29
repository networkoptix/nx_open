#pragma once

#include <string>

#include <boost/optional.hpp>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>

bool convertTo(const QString& src, std::string* const dst);

template<typename T>
bool convertTo(const QString& src, boost::optional<T>* const dst)
{
    *dst = T();
    return convertTo(src, &dst->get());
}

/**
 * @return \a false if requested field was not found in \a urlQuery. \a true otherwise.
 */
template<typename T>
bool deserializeFromUrl(
    const QUrlQuery& urlQuery,
    const QString& fieldName,
    T* const value)
{
    if (urlQuery.hasQueryItem(fieldName))
    {
        convertTo(urlQuery.queryItemValue(fieldName), value);
        return true;
    }
    return false;
}

void serializeToUrl(
    QUrlQuery* const urlQuery,
    const QString& fieldName,
    const std::string& value);

template<typename T>
void serializeToUrl(
    QUrlQuery* const urlQuery,
    const QString& fieldName,
    const boost::optional<T>& value)
{
    if (value)
        serializeToUrl(urlQuery, fieldName, value.get());
}
