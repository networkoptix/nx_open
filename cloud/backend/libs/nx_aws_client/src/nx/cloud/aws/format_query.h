#pragma once

#include <QString>

namespace nx::cloud::aws {

QString formatQuery(const std::string& key, const std::string& value);
QString formatQuery(const std::string& key, bool value);

template<
    typename NumericType,
    typename = typename std::enable_if<std::is_arithmetic<NumericType>::value, NumericType>::type>
QString formatQuery(const std::string& key, NumericType value)
{
    return formatQuery(key, std::to_string(value));
}

} // namespace nx::cloud::aws