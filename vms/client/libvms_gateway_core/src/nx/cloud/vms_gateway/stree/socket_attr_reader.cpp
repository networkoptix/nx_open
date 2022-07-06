// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "socket_attr_reader.h"

#include "cdb_ns.h"

namespace nx::cloud::gateway {

SocketResourceReader::SocketResourceReader(const network::AbstractCommunicatingSocket& sock):
    m_socket(sock)
{
}

std::optional<std::string> SocketResourceReader::getStr(
    const nx::utils::stree::AttrName& name) const
{
    if (name == attr::socketIntfIP)
        return m_socket.getLocalAddress().address.toString();
    else if (name == attr::socketRemoteIP)
        return m_socket.getForeignAddress().address.toString();

    return std::nullopt;
}

} // namespace nx::cloud::gateway
