#pragma once

#include <QtCore/QString>

#include <nx/network/resolve/address_entry.h>

namespace nx {
namespace network {
namespace cloud {

struct TunnelAttributes
{
    AddressType addressType = AddressType::unknown;
    QString remotePeerName;
};

} // namespace cloud
} // namespace network
} // namespace nx
