// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#include "http_request_attr_reader.h"

#include "cdb_ns.h"

namespace nx::cloud::gateway {

HttpRequestResourceReader::HttpRequestResourceReader(
    const nx::network::http::Request& request)
    :
    m_request(request)
{
}

std::optional<std::string> HttpRequestResourceReader::getStr(
    const nx::utils::stree::AttrName& name) const
{
    if (name == attr::requestPath)
        return m_request.requestLine.url.path().toStdString();

    return std::nullopt;
}

} // namespace nx::cloud::gateway
