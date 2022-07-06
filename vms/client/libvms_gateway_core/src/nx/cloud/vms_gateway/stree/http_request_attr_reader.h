// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/utils/stree/attribute_dictionary.h>
#include <nx/network/http/http_types.h>

namespace nx::cloud::gateway {

class HttpRequestResourceReader:
    public nx::utils::stree::AbstractAttributeReader
{
public:
    HttpRequestResourceReader(const nx::network::http::Request& request);

    virtual std::optional<std::string> getStr(const nx::utils::stree::AttrName& name) const override;

private:
    const nx::network::http::Request& m_request;
};

} // namespace nx::cloud::gateway
