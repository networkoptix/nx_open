/**********************************************************
* Sep 15, 2015
* a.kolesnikov
***********************************************************/

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
            *value = m_socket.getLocalAddress().address.toString();
            return true;
        case attr::socketRemoteIP:
            *value = m_socket.getForeignAddress().address.toString();
            return true;
        default:
            return false;
    }
}

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
