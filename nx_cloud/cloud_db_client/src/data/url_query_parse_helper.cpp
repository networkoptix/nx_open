#include "url_query_parse_helper.h"

bool convertTo(const QString& src, std::string* const dst)
{
    *dst = src.toStdString();
    return true;
}

void serializeToUrl(
    QUrlQuery* const urlQuery,
    const QString& fieldName,
    const std::string& value)
{
    urlQuery->addQueryItem(fieldName, QString::fromStdString(value));
}
