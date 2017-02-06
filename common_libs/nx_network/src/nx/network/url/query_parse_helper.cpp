#include "query_parse_helper.h"

namespace nx {
namespace network {
namespace url {

namespace detail {

bool convertTo(const QString& src, std::string* const dst)
{
    *dst = src.toStdString();
    return true;
}

} // namespace detail

void serializeField(
    QUrlQuery* const urlQuery,
    const QString& fieldName,
    const std::string& value)
{
    urlQuery->addQueryItem(fieldName, QString::fromStdString(value));
}

} // namespace nx
} // namespace network
} // namespace url
