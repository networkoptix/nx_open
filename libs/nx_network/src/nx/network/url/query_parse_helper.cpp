// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "query_parse_helper.h"

namespace nx::network::url {

namespace detail {

bool convertTo(const std::string& src, std::string* const dst)
{
    *dst = src;
    return true;
}

} // namespace detail

void serializeField(
    QUrlQuery* const urlQuery,
    const std::string& fieldName,
    const std::string& value)
{
    urlQuery->addQueryItem(
        QString::fromStdString(fieldName),
        QString::fromStdString(value));
}

} // namespace nx::network::url
