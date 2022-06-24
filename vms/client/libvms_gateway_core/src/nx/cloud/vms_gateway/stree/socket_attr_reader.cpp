// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "socket_attr_reader.h"

#include "cdb_ns.h"


namespace nx {
namespace cloud {
namespace gateway {

SocketResourceReader::SocketResourceReader(const network::AbstractCommunicatingSocket& sock)
:
    m_socket(sock)
{
}

bool SocketResourceReader::getAsVariant(int resID, QVariant* const value) const
{
    switch (resID)
    {
        case attr::socketIntfIP:
            *value = QString::fromStdString(m_socket.getLocalAddress().address.toString());
            return true;
        case attr::socketRemoteIP:
            *value = QString::fromStdString(m_socket.getForeignAddress().address.toString());
            return true;
        default:
            return false;
    }
}

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
