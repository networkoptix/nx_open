#include "format_query.h"

namespace nx::cloud::aws {

QString formatQuery(const std::string& key, const std::string& value)
{
    QString query;
    query.reserve(key.size() + value.size() + 1);
    return query.append(key.c_str()).append("=").append(value.c_str());
}

QString formatQuery(const std::string& key, bool value)
{
    return formatQuery(key, value ? std::string("true") : std::string("false"));
}

} // namespace nx::cloud::aws