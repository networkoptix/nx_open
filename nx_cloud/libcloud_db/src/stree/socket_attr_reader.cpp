/**********************************************************
* Sep 15, 2015
* a.kolesnikov
***********************************************************/

#include "socket_attr_reader.h"

#include "data/cdb_ns.h"


namespace nx {
namespace cdb {

SocketResourceReader::SocketResourceReader(const AbstractCommunicatingSocket& sock)
:
    m_socket(sock)
{
}

bool SocketResourceReader::getAsVariant(int resID, QVariant* const value) const
{
    switch (resID)
    {
        case param::socketIntfIP:
            *value = m_socket.getLocalAddress().address.toString();
            return true;
        case param::socketRemoteIP:
            *value = m_socket.getForeignAddress().address.toString();
            return true;
        default:
            return false;
    }
}

}   //cdb
}   //nx
