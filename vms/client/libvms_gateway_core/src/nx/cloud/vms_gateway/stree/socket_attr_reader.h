// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/server/request_processing_types.h>
#include <nx/network/socket.h>
#include <nx/utils/stree/attribute_dictionary.h>

namespace nx::cloud::gateway {

class SocketResourceReader:
    public nx::utils::stree::AbstractAttributeReader
{
public:
    SocketResourceReader(const nx::network::http::ConnectionAttrs& attrs);

    virtual std::optional<std::string> getStr(const nx::utils::stree::AttrName& name) const override;

private:
    const nx::network::http::ConnectionAttrs& m_attrs;
};

} // namespace nx::cloud::gateway
