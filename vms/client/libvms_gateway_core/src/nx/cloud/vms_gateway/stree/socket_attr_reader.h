// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    SocketResourceReader(const network::AbstractCommunicatingSocket& sock);

    //!Implementation of \a AbstractResourceReader::getAsVariant
    virtual bool getAsVariant(int resID, QVariant* const value) const override;

private:
    const network::AbstractCommunicatingSocket& m_socket;
};

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
