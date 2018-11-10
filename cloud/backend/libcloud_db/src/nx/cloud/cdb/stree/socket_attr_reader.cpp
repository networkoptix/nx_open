#include "socket_attr_reader.h"

#include "cdb_ns.h"

namespace nx {
namespace cdb {

SocketResourceReader::SocketResourceReader(
    const network::AbstractCommunicatingSocket& sock)
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

} // namespace cdb
} // namespace nx
