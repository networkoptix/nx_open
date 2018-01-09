/**********************************************************
* Sep 15, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_STREE_SOCKET_ATTR_READER_H
#define NX_CDB_STREE_SOCKET_ATTR_READER_H

#include <nx/utils/stree/resourcecontainer.h>
#include <nx/network/socket.h>


namespace nx {
namespace cdb {

class SocketResourceReader
:
    public nx::utils::stree::AbstractResourceReader
{
public:
    SocketResourceReader(const network::AbstractCommunicatingSocket& sock);

    //!Implementation of \a AbstractResourceReader::getAsVariant
    virtual bool getAsVariant(int resID, QVariant* const value) const override;

private:
    const network::AbstractCommunicatingSocket& m_socket;
};

}   //cdb
}   //nx

#endif  //NX_CDB_STREE_SOCKET_ATTR_READER_H
