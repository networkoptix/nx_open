#pragma once

#include <nx/utils/stree/resourcecontainer.h>
#include <nx/network/socket.h>

namespace nx {
namespace cdb {

class SocketResourceReader:
    public nx::utils::stree::AbstractResourceReader
{
public:
    SocketResourceReader(const network::AbstractCommunicatingSocket& sock);

    virtual bool getAsVariant(int resID, QVariant* const value) const override;

private:
    const network::AbstractCommunicatingSocket& m_socket;
};

} // namespace cdb
} // namespace nx
