/**********************************************************
* Sep 15, 2015
* a.kolesnikov
***********************************************************/

#pragma once

#include <nx/utils/stree/resourcecontainer.h>
#include <nx/network/socket.h>


namespace nx {
namespace cloud {
namespace gateway {

class SocketResourceReader
:
    public nx::utils::stree::AbstractResourceReader
{
public:
    SocketResourceReader(const AbstractCommunicatingSocket& sock);

    //!Implementation of \a AbstractResourceReader::getAsVariant
    virtual bool getAsVariant(int resID, QVariant* const value) const override;

private:
    const AbstractCommunicatingSocket& m_socket;
};

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
