// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/stree/attribute_dictionary.h>
#include <nx/network/socket.h>

namespace nx::cloud::gateway {

class SocketResourceReader:
    public nx::utils::stree::AbstractAttributeReader
{
public:
    SocketResourceReader(const network::AbstractCommunicatingSocket& sock);

    virtual std::optional<std::string> getStr(const nx::utils::stree::AttrName& name) const override;

private:
    const network::AbstractCommunicatingSocket& m_socket;
};

} // namespace nx::cloud::gateway
